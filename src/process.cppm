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

enum class stdio { inherit, pipe, null };

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

    result<exit_status> wait();
    result<std::optional<exit_status>> try_wait();
    result<std::optional<exit_status>> wait_for(std::chrono::milliseconds timeout);

    // A request to end the program, which it may observe and decline.
    void terminate();
    // The program's unit, or the program, ended outright.
    void kill();
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

}  // namespace mcpplibs::os::process
