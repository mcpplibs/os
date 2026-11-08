// mcpplibs.os.process --- starting programs, speaking to them, and ending them.
//
// Composed of openkal.process and openkal.timeout (part 2 §4): the argument
// vector arrives unaltered, the started program receives the three streams and
// nothing else, a unit ends with everything its members started, and a bound
// lifetime ends a started program with this one. The convenience here is the
// composition --- a program found on PATH, an environment stated as overrides,
// pipes made and closed in the right order, both outputs read at once.
export module mcpplibs.os.process;

import std;
export import mcpplibs.os.error;

export namespace mcpplibs::os::process {

// What a started program's stream is: this program's own, a pipe to this
// program, the null device, (`error') this program's standard error --- for a
// program whose standard output carries a protocol, where what a child writes
// may not reach it --- or (`output', for the error stream only) the stream the
// program's standard output is: one pipe carries both, in the order written,
// which is the order a person reads a compiler's diagnostics in.
enum class stdio { inherit, pipe, null, error, output };

// A DIRECTORY A STARTED PROGRAM RECEIVES (openkal 0.23): the name it knows it
// by, the directory, and what it may do beneath it. A program that reaches
// files only through what it was given is held to these by openkal; one
// started with `confine' is held to them by the system as well.
struct grant {
    // The name, normally the directory's absolute path, by which the program
    // resolves an absolute name beneath it.
    std::string name;
    std::filesystem::path path;
    // Nothing beneath is created, written, removed or renamed.
    bool read_only = false;
    // The caller states that nothing beneath changes while the program runs,
    // so the implementation may keep what it read of it.
    bool unchanging = false;
    // Each name the program reaches beneath, and whether it was there, is
    // appended to this file (openkal's record: `r', `w', `s', `a', `l' and the
    // name, then `e' when the program ends); empty for none.
    std::filesystem::path record;
};

// What a started program spends its machine's resources for (openkal 0.23).
enum class resources { unstated, balanced, performance, economy };

// A program identity, obtained from self/describe rather than inferred.
struct program {
    std::uint64_t id = 0;
    std::uint64_t started_ns = 0;
    auto operator<=>(const program&) const = default;
};

// An observation is deliberately three-valued. A lost native reference is
// unknown, and cannot authorize output recovery or reuse of a compiler slot.
enum class unit_state { unknown, occupied, empty };
class process;

class unit_observer {
public:
    static result<unit_observer> open(std::string_view reference);
    unit_observer() = default;
    unit_observer(unit_observer&&) noexcept;
    unit_observer& operator=(unit_observer&&) noexcept;
    unit_observer(const unit_observer&) = delete;
    unit_observer& operator=(const unit_observer&) = delete;
    ~unit_observer();
    result<unit_state> observe() const;
private:
    std::uintptr_t job_ = 0;
    std::string reference_;
};

// The calling program declares its unit before starting work in it. Its
// reference can be retained by another owner before that work is admitted.
// Closing the object releases a reference and ends no member.
class owned_unit {
public:
    static result<owned_unit> create();
    owned_unit();
    owned_unit(owned_unit&&) noexcept;
    owned_unit& operator=(owned_unit&&) noexcept;
    owned_unit(const owned_unit&) = delete;
    owned_unit& operator=(const owned_unit&) = delete;
    ~owned_unit();
    const std::string& reference() const;
    // Seals admission, then asks whether every external member has ended.
    // Occupied/unknown can be retried. Future spawns into this unit fail.
    // The caller remains alive and may finish its own bookkeeping in the
    // continuation's unit. Its full identity is checked; no stale group is used.
    // Dead, reused or inaccessible continuations yield unknown. This conveys
    // no admission or termination right over the continuation's unit.
    result<unit_state> retire(const program& continuation);
    // Ends every member outright, including this program when it is a member.
    void terminate();
private:
    friend class process;
    struct state;
    std::shared_ptr<state> state_;
};

struct spawn_options {
    // An absolute name, or a bare one looked for on PATH (`env::find_executable').
    std::filesystem::path program;
    // The arguments after the first; the first is `argv0', or the program's name.
    std::vector<std::string> args;
    std::optional<std::string> argv0;
    // Where the program runs; empty for this program's own working directory.
    std::filesystem::path cwd;
    // This program's environment, with each override applied: a value sets
    // the variable and nothing removes it. `clear_env' starts from nothing.
    std::vector<std::pair<std::string, std::optional<std::string>>> env;
    bool clear_env = false;
    stdio in = stdio::inherit, out = stdio::pipe, err = stdio::pipe;
    // A unit of the program and everything it starts, which `kill' ends.
    bool own_unit = false;
    // The program ends when this one ends, however this one ends.
    bool bound_lifetime = false;
    // Where a program receives one string and splits it by rules of its own
    // (Windows), the arguments are joined after the program's name as they are,
    // unquoted: for the command interpreter, whose `/s /c' takes the rest of
    // the line as a command of its own grammar (0.2.0, openkal 0.17.1).
    // errc::not_supported where a program receives a vector.
    bool one_string = false;
    // The directories the program receives, the first being the one it
    // regards as the one it was started in; none given, those the
    // implementation supplies by default. errc::not_supported where the
    // implementation grants no directories, or a right asked for cannot be
    // held (0.2.8, openkal 0.23).
    std::optional<std::vector<grant>> grants;
    // The program is held to its grants by the system, not by openkal alone;
    // errc::not_supported where the system cannot hold it.
    bool confine = false;
    resources preference = resources::unstated;
    // Explicitly declared unit, mutually exclusive with own_unit. It must not
    // have begun retirement. end_unit/kill then end this entire unit, including
    // its owner if the owner is a member.
    owned_unit* declared_unit = nullptr;
};

// How a program ended: the status it returned, or, ended by its environment
// (a signal, a console interrupt, a forced end), that condition's number.
struct exit_status {
    int  code = 0;
    bool by_environment = false;
    bool success() const noexcept { return !by_environment && code == 0; }
    // The number a POSIX shell reports for it (128 + signal when ended so).
    int shell_code() const noexcept { return by_environment ? 128 + (code & 0x7f) : code; }
};

class process {
public:
    static result<process> spawn(const spawn_options& options);

    process();
    process(process&&) noexcept;
    process& operator=(process&&) noexcept;
    process(const process&) = delete;
    process& operator=(const process&) = delete;
    // A program still running when its handle is dropped is ended and awaited,
    // unless it was detached.
    ~process();

    bool valid() const noexcept;
    // The identifier the system gives the program, for a report or a lease.
    std::uint64_t id() const;

    // All of it, or the condition that stopped it (errc::closed when the
    // program no longer reads). Safe from several threads.
    result<void> write(std::string_view bytes);
    void close_input();

    // One chunk of what the program wrote; empty at the end.
    result<std::string> read_output();
    result<std::string> read_error();
    // The same, giving up after `timeout': nothing when it expired.
    result<std::optional<std::string>> read_output_for(std::chrono::milliseconds timeout);
    result<std::optional<std::string>> read_error_for(std::chrono::milliseconds timeout);
    // False once a bounded read met a stream the system cannot bound and read
    // it unbounded instead: a thread waiting there cannot be stopped by its
    // bound, only by the stream's end.
    bool reads_bounded() const noexcept;

    result<exit_status> wait();
    result<std::optional<exit_status>> try_wait();
    result<std::optional<exit_status>> wait_for(std::chrono::milliseconds timeout);

    // A request to end the program, which it may observe and decline.
    void terminate();
    // The program's unit, or the program, ended outright.
    void kill();
    // Everything else in the program's unit ended, whether or not the program
    // itself still runs: a program that left something behind (a descendant
    // holding a pipe open) is exactly the case where it has ended and its unit
    // has not. Nothing happens for a program without a unit.
    void end_unit();
    // Released, and left running.
    void detach();

private:
    struct state;
    std::unique_ptr<state> state_;
};

// Starts a program, gives it `input', and collects both outputs until it ends.
struct output {
    exit_status status;
    std::string out;
    std::string err;
};
result<output> run(spawn_options options, std::string_view input = {});

// A RUN WITH BOUNDS (0.2.1), for a program that may not end, may leave a
// descendant holding its output open, or may write without end. It runs in a
// unit of its own. Past `hard' it is asked to end and, `grace' later, its unit
// is ended. Its outputs are read for `drain' after it has ended; one a
// descendant still holds open then ends the unit as well (`held_open'). Each
// output keeps at most its limit, and what is past it is read and dropped, so
// the program never waits on a full pipe.
struct bounds {
    std::chrono::milliseconds hard  { std::chrono::seconds { 60 } };
    std::chrono::milliseconds grace { std::chrono::seconds { 2 } };
    std::chrono::milliseconds drain { std::chrono::seconds { 1 } };
    std::size_t out_limit = std::size_t { 256 } << 20;
    std::size_t err_limit = std::size_t { 1 } << 20;
};
struct bounded_output {
    exit_status status;
    std::string out;
    std::string err;
    bool timed_out     = false;  // `hard' passed and the program was ended
    bool held_open     = false;  // an output stayed open after the program ended
    bool out_truncated = false;
    bool err_truncated = false;
    std::chrono::milliseconds duration { 0 };
};
result<bounded_output> run(spawn_options options, const bounds& limits, std::string_view input = {});

// Becomes the program: returns only when it could not be started. Where the
// system replaces a program's image (Linux, macOS) this program's image is
// replaced, keeping its identity, its terminal and its place among the
// processes that wait for it; elsewhere the program is started with this
// program's three streams, in a unit bound to this program, and this program
// ends as it ended. Either way the status a caller sees is the program's own.
// Streams other than `inherit' are refused: nothing remains to read them.
result<void> exec(spawn_options options);

// --- programs this one did not start (openkal 0.17) ------------------------

// A program, named so that a later one given the same identifier is another.


struct program_info {
    program       who;
    std::uint64_t cpu_ns = 0;
    bool          ended = false;
};

// WHAT A PROGRAM HAS USED (0.4.1): processor time, the largest resident set and
// the bytes moved by its reads and writes. `of::self' is this program so far;
// `of::children' the programs it started and has waited for, together, as the
// kernel accounts them (Linux and macOS). A runner that starts one program and
// asks for its children has that program's use. A field the system does not
// tell is absent; errc::not_supported where nothing is told (an image, the Web,
// a Windows program's children).
enum class of { self, children };
struct usage {
    std::chrono::nanoseconds user{0}, system{0};
    std::optional<std::uint64_t> peak_memory;     // bytes
    std::optional<std::uint64_t> read_bytes, written_bytes;
};
result<usage> resources(of who = of::self);

// This program.
result<program> self();
// What is known of the program with this identifier now; errc::not_found
// when there is none.
result<program_info> describe(std::uint64_t id);
// Whether that program is still the one that was named.
bool alive(const program& p);

// THE THREADS OF A PROGRAM and the processor time each has used (0.2.1), where
// the system tells them apart (Linux: /proc/<id>/task); none elsewhere, where
// `describe' gives the program's whole time.
struct thread_info {
    std::uint64_t id = 0;
    std::string   name;
    std::uint64_t cpu_ns = 0;
};
std::vector<thread_info> threads(std::uint64_t id);

}  // namespace mcpplibs::os::process

// Each system's own (src/<os>/exec.cpp): the image replaced by the program at
// `program', or errc::not_supported where the system does not replace one.
namespace mcpplibs::os::process::sys {
result<void> replace_image(const std::filesystem::path& program,
                           const std::vector<std::string>& argv,
                           const std::vector<std::string>& environment,
                           const std::filesystem::path& cwd);
// What `resources' reports, asked of the system (src/<os>/exec.cpp).
result<usage> resources(of who);
}  // namespace mcpplibs::os::process::sys
