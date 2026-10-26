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

}  // namespace mcpplibs::os::linux
