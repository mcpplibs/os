// mcpplibs.os.linux --- what Linux has and the others do not. Declared on every
// target and defined only on Linux (R5): a use outside `if constexpr
// (os::is_linux)' on another system is a link error, never a branch at run time.
export module mcpplibs.os.linux;

import std;

export namespace mcpplibs::os::linux {

// The system-call translator this program runs under, by name (`proot'), for a
// report of why something behaves as it does; nothing when there is none. There
// is no portable notion of a translator, which is why this is here.
std::optional<std::string> translator();

// What `translator' reads, and its answer from them: this program's
// /proc/self/status, the name of the program tracing it (/proc/<TracerPid>/comm,
// when there is one and it can be read), and the environment as `NAME=value'.
// A tracer named proot* is PRoot and another tracer (a debugger) is none; the
// PROOT_ variables decide only when the tracer cannot be read (a hidden
// /proc), and in a program nothing traces they mean nothing.
struct translator_facts {
    std::string              status;
    std::string              tracer_name;
    std::vector<std::string> environment;
};
std::optional<std::string> classify_translator(const translator_facts& facts);

}  // namespace mcpplibs::os::linux
