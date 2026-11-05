module mcpplibs.os.process;

import std;
import openkal.types;
import openkal.fs;
import openkal.stream;
import openkal.process;
import openkal.timeout;
import mcpplibs.os.platform;
import mcpplibs.os.error;
import mcpplibs.os.env;
import mcpplibs.os.fs;

namespace mcpplibs::os::process {

namespace {

kal_dir as_dir(std::uintptr_t w) { return kal_dir{ static_cast<kal_uintptr>(w) }; }

bool same_name(std::string_view a, std::string_view b, bool fold) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto x = static_cast<unsigned char>(a[i]), y = static_cast<unsigned char>(b[i]);
        if (fold) { x = static_cast<unsigned char>(std::tolower(x)); y = static_cast<unsigned char>(std::tolower(y)); }
        if (x != y) return false;
    }
    return true;
}

// This program's environment with the overrides applied, as `NAME=value'.
std::vector<std::string> environment_for(const spawn_options& o) {
    std::vector<std::string> out = o.clear_env ? std::vector<std::string>{} : env::variables();
    const bool fold = host() == platform::windows;
    for (const auto& [name, value] : o.env) {
        std::erase_if(out, [&](const std::string& e) {
            return same_name(std::string_view(e).substr(0, e.find('=')), name, fold);
        });
        if (value) out.push_back(name + "=" + *value);
    }
    return out;
}

exit_status status_of(int status, int terminated) { return exit_status{ status, terminated != 0 }; }

kal_u64 nanoseconds(std::chrono::milliseconds t) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t).count();
    return static_cast<kal_u64>(ns < 1 ? 1 : ns);
}

// Where a program's stream goes when it is to go nowhere, for an
// implementation that does not give the system's null device at a position
// (before openkal 0.22): a file of the system's own where there is one;
// otherwise a pipe whose far end this program closes (input) or drains
// (output). From 0.22 the start is asked for the device itself
// (`KAL_SPAWN_NULL_*`, process::spawn).
struct null_stream {
    kal_stream child{};
    bool       channel = false;    // `child' is an end this program must release
    kal_file   file{};
};

result<null_stream> open_null(bool for_input) {
    null_stream n;
    if (host() != platform::windows) {
        if (auto l = fs::locate("/dev/null")) {
            const auto flags = for_input ? kal::fs::open::read : kal::fs::open::write;
            if (kal_fs_open(as_dir(l->directory), l->relative.data(), l->relative.size(), flags.bits, &n.file) == kal_ok) {
                n.child = kal_fs_stream(n.file);
                return n;
            }
        }
    }
    kal_stream mine{}, theirs{};
    if (const int rc = kal_process_channel(&mine, &theirs); rc != kal_ok)
        return std::unexpected(from_kal(rc, "making a stream that goes nowhere"));
    n.channel = true;
    if (for_input) {
        n.child = mine;                       // the program reads, and meets the end at once
        kal_process_channel_close(theirs);
    } else {
        n.child = theirs;                     // the program writes, and a thread discards it
        std::thread([mine] {
            char sink[4096];
            while (kal_stream_read(mine, sink, sizeof sink) > 0) {}
            kal_process_channel_close(mine);
        }).detach();
    }
    return n;
}

result<std::string> read_chunk(kal_stream s, bool open) {
    if (!open) return std::string{};
    char buf[16384];
    const kal_intptr n = kal_stream_read(s, buf, sizeof buf);
    if (n < 0) {
        if (-n == kal_err_closed) return std::string{};
        return std::unexpected(from_kal(static_cast<int>(-n), "reading a program's output"));
    }
    return std::string(buf, static_cast<std::size_t>(n));
}

result<std::optional<std::string>> read_chunk_for(kal_stream s, bool open, std::chrono::milliseconds t,
                                                   std::atomic<bool>& bounded) {
    if (!open) return std::optional<std::string>{ std::string{} };
    char buf[16384];
    const kal_intptr n = kal_timeout_read(s, buf, sizeof buf, nanoseconds(t));
    if (n < 0) {
        const int code = static_cast<int>(-n);
        if (code == kal_err_again) return std::optional<std::string>{};
        if (code == kal_err_closed) return std::optional<std::string>{ std::string{} };
        if (code == kal_err_not_supported) {
            bounded = false;
            auto r = read_chunk(s, open);
            if (!r) return std::unexpected(r.error());
            return std::optional<std::string>{ std::move(*r) };
        }
        return std::unexpected(from_kal(code, "reading a program's output"));
    }
    return std::optional<std::string>{ std::string(buf, static_cast<std::size_t>(n)) };
}

}  // namespace

namespace {
unit_state unit_state_of(kal_u32 value) {
    return value == kal::process::job_state::empty ? unit_state::empty
         : value == kal::process::job_state::occupied ? unit_state::occupied : unit_state::unknown;
}
}
struct owned_unit::state {
    kal_job job{};
    std::string reference;
    bool sealed = false;
    std::mutex admission;
    ~state() { if (job.h) kal_process_job_close(job); }
};
owned_unit::owned_unit() = default;
owned_unit::owned_unit(owned_unit&&) noexcept = default;
owned_unit& owned_unit::operator=(owned_unit&&) noexcept = default;
owned_unit::~owned_unit() = default;
result<owned_unit> owned_unit::create() {
    auto s = std::make_shared<state>();
    std::string reference(512, '\0'); kal_uintptr used = 0;
    const int rc = kal_process_job_create(&s->job,reference.data(),reference.size(),&used);
    if (rc != kal_ok) return std::unexpected(from_kal(rc,"declaring a process unit"));
    if (used == 0 || used > reference.size()) return std::unexpected(error{errc::io,"invalid process unit reference"});
    reference.resize(used); s->reference = std::move(reference);
    owned_unit result; result.state_ = std::move(s); return result;
}
const std::string& owned_unit::reference() const {
    static const std::string empty;
    return state_ ? state_->reference : empty;
}
result<unit_state> owned_unit::retire() {
    if (!state_) return std::unexpected(error{errc::invalid,"retiring an empty process unit"});
    std::lock_guard guard(state_->admission); state_->sealed = true;
    kal_u32 observed = kal::process::job_state::unknown;
    const int rc = kal_process_job_retire(state_->job,state_->reference.data(),state_->reference.size(),&observed);
    if (rc != kal_ok) return std::unexpected(from_kal(rc,"retiring a process unit"));
    return unit_state_of(observed);
}
void owned_unit::terminate() { if (state_) kal_process_job_terminate(state_->job); }
result<unit_observer> unit_observer::open(std::string_view reference) {
    kal_job job{};
    const int rc = kal_process_job_open(reference.data(),reference.size(),&job);
    if (rc != kal_ok) return std::unexpected(from_kal(rc,"opening a process unit reference"));
    unit_observer result; result.job_ = job.h; result.reference_ = reference; return result;
}
unit_observer::unit_observer(unit_observer&& other) noexcept
    : job_(std::exchange(other.job_,0)), reference_(std::move(other.reference_)) {}
unit_observer& unit_observer::operator=(unit_observer&& other) noexcept {
    if (this != &other) {
        if (job_) kal_process_job_close(kal_job{job_});
        job_ = std::exchange(other.job_,0); reference_ = std::move(other.reference_);
    }
    return *this;
}
unit_observer::~unit_observer() { if (job_) kal_process_job_close(kal_job{job_}); }
result<unit_state> unit_observer::observe() const {
    if (!job_) return std::unexpected(error{errc::invalid,"observing an empty process unit reference"});
    kal_u32 observed = kal::process::job_state::unknown;
    const int rc = kal_process_job_observe(kal_job{job_},reference_.data(),reference_.size(),&observed);
    if (rc != kal_ok) return std::unexpected(from_kal(rc,"observing a process unit"));
    return unit_state_of(observed);
}

struct process::state {
    kal_process handle{};
    kal_stream  input{}, output{}, error{};
    bool        input_open = false, output_open = false, error_open = false;
    kal_job     job{};
    bool        has_job = false;
    std::shared_ptr<owned_unit::state> declared;
    std::optional<exit_status> ended;
    std::atomic<bool> bounded{ true };
    std::mutex  write_lock, wait_lock;
};

process::process() = default;
process::process(process&&) noexcept = default;
process& process::operator=(process&&) noexcept = default;

process::~process() {
    if (!state_) return;
    close_input();
    if (!state_->ended) {
        if (state_->has_job) kal_process_job_terminate(state_->job);
        else kal_process_terminate(state_->handle);
        int status = 0, terminated = 0;
        kal_process_wait(state_->handle, &status, &terminated);
    }
    if (state_->output_open) kal_process_channel_close(state_->output);
    if (state_->error_open) kal_process_channel_close(state_->error);
    if (state_->has_job) kal_process_job_close(state_->job);
    kal_process_close(state_->handle);
}

bool process::valid() const noexcept { return static_cast<bool>(state_); }

std::uint64_t process::id() const {
    if (!state_) return 0;
    kal_program_info info{};
    info.self_size = sizeof info;
    if (kal_process_info(state_->handle, kal::process::info::id, &info) != kal_ok) return 0;
    return info.id;
}

result<process> process::spawn(const spawn_options& o) {
    if (o.in == stdio::error || o.in == stdio::output)
        return std::unexpected(error{ errc::invalid, "a program's input cannot be an output of this program" });
    if (o.out == stdio::output)
        return std::unexpected(error{ errc::invalid, "a program's output cannot be its own output" });
    // The program, by name or by its place.
    std::filesystem::path program = o.program;
    if (!fs::names_a_place(fs::generic_utf8(o.program))) {
        auto found = env::find_executable(fs::generic_utf8(program));
        if (!found) return std::unexpected(error{ errc::not_found, std::format("{} is not on PATH", fs::generic_utf8(program)) });
        program = *found;
    }
    std::error_code ec;
    program = fs::absolute(program).lexically_normal();
    const auto where = fs::locate(program);
    if (!where) return std::unexpected(error{ errc::not_found,
        std::format("starting {}: beneath no directory this program was given", fs::generic_utf8(program)) });

    const auto cwd_path = o.cwd.empty() ? std::filesystem::current_path(ec) : o.cwd;
    const auto cwd = fs::locate(cwd_path);
    if (!cwd) return std::unexpected(error{ errc::not_found,
        std::format("starting {}: {} is beneath no directory this program was given",
                    fs::generic_utf8(program), fs::generic_utf8(cwd_path)) });
    kal_dir work = as_dir(cwd->directory);
    bool close_work = false;
    if (cwd->relative != ".") {
        if (const int rc = kal_fs_open_dir(work, cwd->relative.data(), cwd->relative.size(), &work); rc != kal_ok)
            return std::unexpected(from_kal(rc, std::format("starting {} in {}", fs::generic_utf8(program), fs::generic_utf8(cwd_path))));
        close_work = true;
    }
    struct closer { kal_dir d; bool on; ~closer() { if (on) kal_fs_close_dir(d); } } work_guard{ work, close_work };

    auto s = std::make_unique<state>();
    std::vector<kal_stream> release;                 // the program's ends, released after the start
    std::vector<null_stream> nulls;
    // THE SYSTEM'S NULL DEVICE AT A POSITION (openkal 0.22): asked of the start
    // where the implementation states it, so nothing here holds a pipe or a
    // thread for a program whose stream goes nowhere.
    const bool device = (kal_process_props() & kal::process::null_streams.bits) != 0;
    kal_uintptr null_at = 0;
    kal_stream child_in{}, child_out{}, child_err{};
    auto fail_streams = [&](const error& e) {
        for (auto st : release) kal_process_channel_close(st);
        if (s->input_open) kal_process_channel_close(s->input);
        if (s->output_open) kal_process_channel_close(s->output);
        if (s->error_open) kal_process_channel_close(s->error);
        return std::unexpected(e);
    };
    // A channel is one way: the first end reads what the second writes.
    if (o.in == stdio::pipe) {
        if (const int rc = kal_process_channel(&child_in, &s->input); rc != kal_ok)
            return fail_streams(from_kal(rc, "making the program's input"));
        s->input_open = true; release.push_back(child_in);
    } else if (o.in == stdio::null && device) {
        null_at |= kal::process::null_in_flag.bits;
    } else if (o.in == stdio::null) {
        auto n = open_null(true);
        if (!n) return fail_streams(n.error());
        child_in = n->child; nulls.push_back(*n);
    }
    if (o.out == stdio::pipe) {
        if (const int rc = kal_process_channel(&s->output, &child_out); rc != kal_ok)
            return fail_streams(from_kal(rc, "making the program's output"));
        s->output_open = true; release.push_back(child_out);
    } else if (o.out == stdio::null && device) {
        null_at |= kal::process::null_out_flag.bits;
    } else if (o.out == stdio::null) {
        auto n = open_null(false);
        if (!n) return fail_streams(n.error());
        child_out = n->child; nulls.push_back(*n);
    } else if (o.out == stdio::error) {
        child_out = kal_stderr();
    }
    if (o.err == stdio::output) {
        if (o.out == stdio::inherit) child_err = kal_stdout();
        else if (null_at & kal::process::null_out_flag.bits) null_at |= kal::process::null_err_flag.bits;
        else child_err = child_out;
    } else if (o.err == stdio::pipe) {
        if (const int rc = kal_process_channel(&s->error, &child_err); rc != kal_ok)
            return fail_streams(from_kal(rc, "making the program's error output"));
        s->error_open = true; release.push_back(child_err);
    } else if (o.err == stdio::null && device) {
        null_at |= kal::process::null_err_flag.bits;
    } else if (o.err == stdio::null) {
        auto n = open_null(false);
        if (!n) return fail_streams(n.error());
        child_err = n->child; nulls.push_back(*n);
    } else if (o.err == stdio::error) {
        child_err = kal_stderr();
    }

    // The vector, the first element in the system's own spelling: a program
    // that reads its own command line takes a forward slash for a switch.
    std::string first = o.argv0.value_or(fs::utf8(program));
    if (!o.argv0 && host() == platform::windows) std::ranges::replace(first, '/', '\\');
    std::vector<std::string> argv_storage{ std::move(first) };
    for (const auto& a : o.args) argv_storage.push_back(a);
    std::vector<const char*> argv; std::vector<kal_uintptr> argv_lens;
    for (const auto& a : argv_storage) { argv.push_back(a.data()); argv_lens.push_back(a.size()); }

    const auto environment = environment_for(o);
    std::vector<const char*> envp; std::vector<kal_uintptr> envp_lens;
    for (const auto& e : environment) { envp.push_back(e.data()); envp_lens.push_back(e.size()); }

    const kal_uintptr props = kal_process_props();
    const bool bind = o.bound_lifetime && (props & kal::process::bound_lifetime.bits) != 0;
    if (o.one_string && (props & kal::process::one_string.bits) == 0)
        return fail_streams(error{ errc::not_supported,
            std::format("starting {}: a program here receives a vector, not one string", fs::generic_utf8(program)) });
    // THE DIRECTORIES THE PROGRAM RECEIVES (0.2.8, openkal 0.23), each opened
    // and given the rights asked for, and released once the program started.
    struct granted {
        std::vector<kal_dir> dirs;
        std::vector<kal_file> records;
        std::vector<kal_preopen> preopens;
        ~granted() {
            for (auto d : dirs) kal_fs_close_dir(d);
            for (auto f : records) kal_fs_close_file(f);
        }
    } g;
    if (o.grants) {
        if ((props & kal::process::grant_dir.bits) == 0)
            return fail_streams(error{ errc::not_supported,
                std::format("starting {}: this system grants a started program no directories", fs::generic_utf8(program)) });
        g.preopens.reserve(o.grants->size());
        for (const auto& gr : *o.grants) {
            const auto at = fs::locate(fs::absolute(gr.path).lexically_normal());
            if (!at) return fail_streams(error{ errc::not_found,
                std::format("granting {}: beneath no directory this program was given", fs::generic_utf8(gr.path)) });
            kal_dir d{};
            if (const int rc = kal_fs_open_dir(as_dir(at->directory), at->relative.data(), at->relative.size(), &d); rc != kal_ok)
                return fail_streams(from_kal(rc, std::format("granting {}", fs::generic_utf8(gr.path))));
            g.dirs.push_back(d);
            kal_dir_rights rights{};
            rights.self_size = sizeof rights;
            if (gr.read_only) rights.flags |= kal::fs::dir_read_only.bits;
            if (gr.unchanging) rights.flags |= kal::fs::dir_unchanging.bits;
            if (!gr.record.empty()) {
                const auto r = fs::locate(fs::absolute(gr.record).lexically_normal());
                if (!r) return fail_streams(error{ errc::not_found,
                    std::format("recording {}: beneath no directory this program was given", fs::generic_utf8(gr.record)) });
                kal_file rec{};
                const auto how_open = kal::fs::open::write | kal::fs::open::create | kal::fs::open::append;
                if (const int rc = kal_fs_open(as_dir(r->directory), r->relative.data(), r->relative.size(), how_open.bits, &rec); rc != kal_ok)
                    return fail_streams(from_kal(rc, std::format("recording {}", fs::generic_utf8(gr.record))));
                g.records.push_back(rec);
                rights.flags |= kal::fs::dir_recorded.bits;
                rights.record = rec;
            }
            if (rights.flags != 0) {
                if ((kal_fs_props(d) & kal::fs::attenuate.bits) == 0)
                    return fail_streams(error{ errc::not_supported,
                        std::format("granting {}: this system cannot hold a directory to rights", fs::generic_utf8(gr.path)) });
                kal_dir held{};
                if (const int rc = kal_fs_dir_attenuate(d, &rights, &held); rc != kal_ok)
                    return fail_streams(from_kal(rc, std::format("granting {}", fs::generic_utf8(gr.path))));
                g.dirs.push_back(held);
                d = held;
            }
            g.preopens.push_back(kal_preopen{ d, gr.name.data(), gr.name.size() });
        }
    }
    if (o.confine && (props & kal::process::confine.bits) == 0)
        return fail_streams(error{ errc::not_supported,
            std::format("starting {}: this system cannot hold a program to its grants", fs::generic_utf8(program)) });
    const kal_uintptr preference =
          o.preference == resources::balanced    ? kal::process::balanced_flag.bits
        : o.preference == resources::performance ? kal::process::performance_flag.bits
        : o.preference == resources::economy     ? kal::process::economy_flag.bits : kal_uintptr{ 0 };
    kal_spawn how{ as_dir(where->directory), work, nullptr,
                   o.grants ? g.preopens.data() : nullptr, o.grants ? g.preopens.size() : 0,
                   (bind ? kal::process::bound_lifetime_flag.bits : kal_uintptr{ 0 })
                   | (o.one_string ? kal::process::one_string_flag.bits : kal_uintptr{ 0 })
                   | (o.confine ? kal::process::confine_flag.bits : kal_uintptr{ 0 })
                   | preference | null_at };
    std::unique_lock<std::mutex> admission;
    if (o.declared_unit) {
        if (o.own_unit || !o.declared_unit->state_)
            return fail_streams(error{errc::invalid,"a declared process unit conflicts with own_unit or is empty"});
        s->declared = o.declared_unit->state_;
        admission = std::unique_lock(s->declared->admission);
        if (s->declared->sealed)
            return fail_streams(error{errc::closed,"a retired process unit accepts no new program"});
    }
    kal_job job{};
    const bool unit = o.own_unit && (props & kal::process::job.bits) != 0;
    if (unit) how.job = &job;
    else if (s->declared) how.job = &s->declared->job;
    const kal_spawn_streams streams{ child_in, child_out, child_err };

    const int rc = kal_process_spawn(&how, where->relative.data(), where->relative.size(),
                                     argv.data(), argv_lens.data(), argv.size(),
                                     envp.data(), envp_lens.data(), envp.size(),
                                     &streams, &s->handle);
    for (auto st : release) kal_process_channel_close(st);
    release.clear();
    for (auto& n : nulls) {
        if (n.file.h) kal_fs_close_file(n.file);
        else if (n.channel && n.child.h) kal_process_channel_close(n.child);
    }
    if (rc != kal_ok) return fail_streams(from_kal(rc, std::format("starting {}", fs::generic_utf8(program))));
    if (unit) { s->job = job; s->has_job = true; }
    process p;
    p.state_ = std::move(s);
    return p;
}

result<void> process::write(std::string_view bytes) {
    if (!state_ || !state_->input_open) return std::unexpected(error{ errc::closed, "writing to a program's input" });
    std::lock_guard g(state_->write_lock);
    std::size_t done = 0;
    while (done < bytes.size()) {
        const kal_intptr n = kal_stream_write(state_->input, bytes.data() + done, bytes.size() - done);
        if (n <= 0) return std::unexpected(from_kal(n < 0 ? static_cast<int>(-n) : kal_err_io, "writing to a program's input"));
        done += static_cast<std::size_t>(n);
    }
    return {};
}

void process::close_input() {
    if (!state_) return;
    std::lock_guard g(state_->write_lock);
    if (state_->input_open) { kal_process_channel_close(state_->input); state_->input_open = false; }
}

result<std::string> process::read_output() {
    if (!state_) return std::string{};
    return read_chunk(state_->output, state_->output_open);
}
result<std::string> process::read_error() {
    if (!state_) return std::string{};
    return read_chunk(state_->error, state_->error_open);
}
result<std::optional<std::string>> process::read_output_for(std::chrono::milliseconds t) {
    if (!state_) return std::optional<std::string>{ std::string{} };
    return read_chunk_for(state_->output, state_->output_open, t, state_->bounded);
}
result<std::optional<std::string>> process::read_error_for(std::chrono::milliseconds t) {
    if (!state_) return std::optional<std::string>{ std::string{} };
    return read_chunk_for(state_->error, state_->error_open, t, state_->bounded);
}

result<exit_status> process::wait() {
    if (!state_) return std::unexpected(error{ errc::invalid, "waiting for no program" });
    std::lock_guard g(state_->wait_lock);
    if (state_->ended) return *state_->ended;
    int status = 0, terminated = 0;
    if (const int rc = kal_process_wait(state_->handle, &status, &terminated); rc != kal_ok)
        return std::unexpected(from_kal(rc, "waiting for a program"));
    state_->ended = status_of(status, terminated);
    return *state_->ended;
}

result<std::optional<exit_status>> process::wait_for(std::chrono::milliseconds t) {
    if (!state_) return std::unexpected(error{ errc::invalid, "waiting for no program" });
    std::lock_guard g(state_->wait_lock);
    if (state_->ended) return std::optional<exit_status>{ *state_->ended };
    int status = 0, terminated = 0;
    const int rc = kal_timeout_wait_process(state_->handle, nanoseconds(t), &status, &terminated);
    if (rc == kal_err_again) return std::optional<exit_status>{};
    if (rc != kal_ok) return std::unexpected(from_kal(rc, "waiting for a program"));
    state_->ended = status_of(status, terminated);
    return std::optional<exit_status>{ *state_->ended };
}

result<std::optional<exit_status>> process::try_wait() { return wait_for(std::chrono::milliseconds(0)); }

void process::terminate() {
    if (!state_) return;
    std::lock_guard g(state_->wait_lock);
    if (!state_->ended) kal_process_terminate(state_->handle);
}

void process::kill() {
    if (!state_) return;
    std::lock_guard g(state_->wait_lock);
    if (state_->ended) return;
    if (state_->declared) kal_process_job_terminate(state_->declared->job);
    else if (state_->has_job) kal_process_job_terminate(state_->job);
    else kal_process_terminate(state_->handle);
}

bool process::reads_bounded() const noexcept { return !state_ || state_->bounded.load(); }

void process::end_unit() {
    if (!state_) return;
    if (state_->declared) kal_process_job_terminate(state_->declared->job);
    else if (state_->has_job) kal_process_job_terminate(state_->job);
}

void process::detach() {
    if (!state_) return;
    close_input();
    if (state_->output_open) kal_process_channel_close(state_->output);
    if (state_->error_open) kal_process_channel_close(state_->error);
    if (state_->has_job) kal_process_job_close(state_->job);
    kal_process_close(state_->handle);
    state_.reset();
}

result<output> run(spawn_options options, std::string_view input) {
    if (!input.empty()) options.in = stdio::pipe;
    options.out = stdio::pipe;
    options.err = stdio::pipe;
    auto p = process::spawn(options);
    if (!p) return std::unexpected(p.error());
    output out;
    std::jthread feeder;
    if (!input.empty()) {
        feeder = std::jthread([&p, input] { (void)p->write(input); p->close_input(); });
    } else {
        p->close_input();
    }
    std::jthread err_reader([&p, &out] {
        for (;;) { auto c = p->read_error(); if (!c || c->empty()) break; out.err += *c; }
    });
    for (;;) { auto c = p->read_output(); if (!c || c->empty()) break; out.out += *c; }
    err_reader.join();
    if (feeder.joinable()) feeder.join();
    auto st = p->wait();
    if (!st) return std::unexpected(st.error());
    out.status = *st;
    return out;
}

namespace {
// One output of a bounded run, read by a thread of its own in slices, so that
// the run can stop it once the program has ended and the drain has passed.
struct capture {
    std::mutex  lock;
    std::string data;
    std::size_t limit = 0;
    bool        truncated = false;
};
struct collection {
    std::mutex              lock;
    std::condition_variable changed;
    int                     open = 2;
    std::atomic<bool>       stop{ false };
};
constexpr std::chrono::milliseconds read_slice{ 50 };

void collect(std::shared_ptr<process> p, std::shared_ptr<capture> into,
             std::shared_ptr<collection> all, bool output) {
    while (!all->stop.load(std::memory_order_relaxed)) {
        auto chunk = output ? p->read_output_for(read_slice) : p->read_error_for(read_slice);
        if (!chunk) break;               // the stream failed: nothing more arrives
        if (!chunk->has_value()) continue;
        if ((*chunk)->empty()) break;    // its end
        std::lock_guard g(into->lock);
        if (into->data.size() >= into->limit) { into->truncated = true; continue; }
        const std::size_t room = into->limit - into->data.size();
        if ((*chunk)->size() > room) {
            into->data.append((*chunk)->data(), room);
            into->truncated = true;
        } else {
            into->data += **chunk;
        }
    }
    {
        std::lock_guard g(all->lock);
        --all->open;
    }
    all->changed.notify_all();
}
}  // namespace

result<bounded_output> run(spawn_options options, const bounds& limits, std::string_view input) {
    options.in = stdio::pipe;
    options.out = stdio::pipe;
    options.err = stdio::pipe;
    options.own_unit = true;
    const auto started = std::chrono::steady_clock::now();
    auto spawned = process::spawn(options);
    if (!spawned) return std::unexpected(spawned.error());
    auto p = std::make_shared<process>(std::move(*spawned));
    if (!input.empty()) (void)p->write(input);
    p->close_input();

    auto out = std::make_shared<capture>();
    out->limit = limits.out_limit;
    auto err = std::make_shared<capture>();
    err->limit = limits.err_limit;
    auto all = std::make_shared<collection>();
    std::thread(collect, p, out, all, true).detach();
    std::thread(collect, p, err, all, false).detach();

    bounded_output result;
    auto waited = p->wait_for(limits.hard);
    if (!waited) return std::unexpected(waited.error());
    if (!waited->has_value()) {
        result.timed_out = true;
        p->terminate();                                   // asked
        (void)p->wait_for(limits.grace);
        p->end_unit();
        auto ended = p->wait();
        result.status = ended ? *ended : exit_status{ -1, true };
    } else {
        result.status = **waited;
    }
    {
        std::unique_lock g(all->lock);
        all->changed.wait_for(g, limits.drain, [&] { return all->open == 0; });
        result.held_open = all->open != 0;
    }
    if (result.held_open) {
        p->end_unit();                                    // the program is gone; its unit is not
        all->stop.store(true, std::memory_order_relaxed);
        std::unique_lock g(all->lock);
        all->changed.wait_for(g, read_slice * 4, [&] { return all->open == 0; });
    }
    {
        std::lock_guard g(out->lock);
        result.out = out->data;
        result.out_truncated = out->truncated;
    }
    {
        std::lock_guard g(err->lock);
        result.err = err->data;
        result.err_truncated = err->truncated;
    }
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    return result;
}

result<void> exec(spawn_options o) {
    if (o.in != stdio::inherit || o.out != stdio::inherit || o.err != stdio::inherit)
        return std::unexpected(error{ errc::invalid,
            "a program this one becomes has this program's own streams" });
    std::filesystem::path program = o.program;
    if (!fs::names_a_place(fs::generic_utf8(o.program))) {
        auto found = env::find_executable(fs::generic_utf8(program));
        if (!found) return std::unexpected(error{ errc::not_found, std::format("{} is not on PATH", fs::generic_utf8(program)) });
        program = *found;
    }
    program = fs::absolute(program).lexically_normal();
    std::vector<std::string> argv{ o.argv0.value_or(fs::utf8(program)) };
    for (const auto& a : o.args) argv.push_back(a);
    if (auto r = sys::replace_image(program, argv, environment_for(o), o.cwd);
        r || r.error().code != errc::not_supported)
        return r;   // returned: the image was not replaced
    // No replacement here: the program runs in a unit bound to this one, and
    // this one ends as it ended.
    o.own_unit = true;
    o.bound_lifetime = true;
    o.program = program;
    auto p = process::spawn(o);
    if (!p) return std::unexpected(p.error());
    auto st = p->wait();
    if (!st) return std::unexpected(st.error());
    std::exit(st->code);
}

result<program> self() {
    kal_process me{};
    if (const int rc = kal_process_self(&me); rc != kal_ok) return std::unexpected(from_kal(rc, "naming this program"));
    kal_program_info info{};
    info.self_size = sizeof info;
    const int rc = kal_process_info(me, kal::process::info::all, &info);
    kal_process_close(me);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, "naming this program"));
    return program{ info.id, info.started_ns };
}

result<program_info> describe(std::uint64_t id) {
    kal_process p{};
    if (const int rc = kal_process_open_id(id, &p); rc != kal_ok)
        return std::unexpected(from_kal(rc, std::format("asking about program {}", id)));
    kal_program_info info{};
    info.self_size = sizeof info;
    const int rc = kal_process_info(p, kal::process::info::all, &info);
    kal_process_close(p);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("asking about program {}", id)));
    return program_info{ program{ info.id, info.started_ns }, info.cpu_ns, info.ended != 0 };
}

bool alive(const program& p) {
    auto d = describe(p.id);
    if (!d || d->ended) return false;
    return p.started_ns == 0 || d->who.started_ns == 0 || d->who.started_ns == p.started_ns;
}

std::vector<thread_info> threads(std::uint64_t id) {
    std::vector<thread_info> out;
    if (host() != platform::linux || id == 0) return out;
    // /proc/<id>/task/<tid>/stat: `<tid> (<name>) <state> ...', utime and stime
    // the 12th and 13th fields after the name, in ticks of USER_HZ (100).
    constexpr std::uint64_t ns_per_tick = 10'000'000;
    const auto number = [](std::string_view text) -> std::optional<std::uint64_t> {
        std::uint64_t v = 0;
        const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
        if (ec != std::errc{} || end != text.data() + text.size()) return std::nullopt;
        return v;
    };
    std::error_code ec;
    const std::filesystem::path tasks = std::format("/proc/{}/task", id);
    for (std::filesystem::directory_iterator it(tasks, ec), end; !ec && it != end; it.increment(ec)) {
        const auto stat = fs::read(it->path() / "stat");
        if (!stat) continue;
        const auto open = stat->find('(');
        const auto close = stat->rfind(')');
        if (open == std::string::npos || close == std::string::npos || close < open || close + 2 >= stat->size()) continue;
        std::vector<std::string_view> fields;
        std::string_view rest = std::string_view(*stat).substr(close + 2);
        while (!rest.empty()) {
            const auto space = rest.find(' ');
            if (const auto field = rest.substr(0, space); !field.empty()) fields.push_back(field);
            rest = space == std::string_view::npos ? std::string_view{} : rest.substr(space + 1);
        }
        if (fields.size() < 13) continue;
        const auto tid = number(std::string_view(*stat).substr(0, open > 0 ? open - 1 : 0));
        const auto user = number(fields[11]);
        const auto system = number(fields[12]);
        if (!tid || !user || !system) continue;
        out.push_back(thread_info{ *tid, stat->substr(open + 1, close - open - 1), (*user + *system) * ns_per_tick });
    }
    return out;
}

}  // namespace mcpplibs::os::process
