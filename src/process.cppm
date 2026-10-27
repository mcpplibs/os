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
struct program {
    std::uint64_t id = 0;
    std::uint64_t started_ns = 0;
    auto operator<=>(const program&) const = default;
};

struct program_info {
    program       who;
    std::uint64_t cpu_ns = 0;
    bool          ended = false;
};

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
}  // namespace mcpplibs::os::process::sys
