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

// Where a program's stream goes when it is to go nowhere. A file of the
// system's own where there is one; otherwise a pipe whose far end this program
// closes (input) or drains (output).
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

struct process::state {
    kal_process handle{};
    kal_stream  input{}, output{}, error{};
    bool        input_open = false, output_open = false, error_open = false;
    kal_job     job{};
    bool        has_job = false;
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
    if (o.in == stdio::error)
        return std::unexpected(error{ errc::invalid, "a program's input cannot be this program's error output" });
    // The program, by name or by its place.
    std::filesystem::path program = o.program;
    if (!fs::names_a_place(o.program.generic_string())) {
        auto found = env::find_executable(program.generic_string());
        if (!found) return std::unexpected(error{ errc::not_found, std::format("{} is not on PATH", program.generic_string()) });
        program = *found;
    }
    std::error_code ec;
    program = fs::absolute(program).lexically_normal();
    const auto where = fs::locate(program);
    if (!where) return std::unexpected(error{ errc::not_found,
        std::format("starting {}: beneath no directory this program was given", program.generic_string()) });

    const auto cwd_path = o.cwd.empty() ? std::filesystem::current_path(ec) : o.cwd;
    const auto cwd = fs::locate(cwd_path);
    if (!cwd) return std::unexpected(error{ errc::not_found,
        std::format("starting {}: {} is beneath no directory this program was given",
                    program.generic_string(), cwd_path.generic_string()) });
    kal_dir work = as_dir(cwd->directory);
    bool close_work = false;
    if (cwd->relative != ".") {
        if (const int rc = kal_fs_open_dir(work, cwd->relative.data(), cwd->relative.size(), &work); rc != kal_ok)
            return std::unexpected(from_kal(rc, std::format("starting {} in {}", program.generic_string(), cwd_path.generic_string())));
        close_work = true;
    }
    struct closer { kal_dir d; bool on; ~closer() { if (on) kal_fs_close_dir(d); } } work_guard{ work, close_work };

    auto s = std::make_unique<state>();
    std::vector<kal_stream> release;                 // the program's ends, released after the start
    std::vector<null_stream> nulls;
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
    } else if (o.in == stdio::null) {
        auto n = open_null(true);
        if (!n) return fail_streams(n.error());
        child_in = n->child; nulls.push_back(*n);
    }
    if (o.out == stdio::pipe) {
        if (const int rc = kal_process_channel(&s->output, &child_out); rc != kal_ok)
            return fail_streams(from_kal(rc, "making the program's output"));
        s->output_open = true; release.push_back(child_out);
    } else if (o.out == stdio::null) {
        auto n = open_null(false);
        if (!n) return fail_streams(n.error());
        child_out = n->child; nulls.push_back(*n);
    } else if (o.out == stdio::error) {
        child_out = kal_stderr();
    }
    if (o.err == stdio::pipe) {
        if (const int rc = kal_process_channel(&s->error, &child_err); rc != kal_ok)
            return fail_streams(from_kal(rc, "making the program's error output"));
        s->error_open = true; release.push_back(child_err);
    } else if (o.err == stdio::null) {
        auto n = open_null(false);
        if (!n) return fail_streams(n.error());
        child_err = n->child; nulls.push_back(*n);
    } else if (o.err == stdio::error) {
        child_err = kal_stderr();
    }

    // The vector, the first element in the system's own spelling: a program
    // that reads its own command line takes a forward slash for a switch.
    std::string first = o.argv0.value_or(program.string());
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
    kal_spawn how{ as_dir(where->directory), work, nullptr, nullptr, 0,
                   bind ? kal::process::bound_lifetime_flag.bits : kal_uintptr{ 0 } };
    kal_job job{};
    const bool unit = o.own_unit && (props & kal::process::job.bits) != 0;
    if (unit) how.job = &job;
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
    if (rc != kal_ok) return fail_streams(from_kal(rc, std::format("starting {}", program.generic_string())));
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
    if (state_->has_job) kal_process_job_terminate(state_->job);
    else kal_process_terminate(state_->handle);
}

bool process::reads_bounded() const noexcept { return !state_ || state_->bounded.load(); }

void process::end_unit() {
    if (!state_ || !state_->has_job) return;
    kal_process_job_terminate(state_->job);
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

}  // namespace mcpplibs::os::process
