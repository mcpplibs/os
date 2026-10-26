module;
#include <sys/sysctl.h>
#include <sys/types.h>

module mcpplibs.os.machine;

import std;

namespace mcpplibs::os::machine {

namespace {
template <class T>
std::optional<T> sysctl_value(const char* name) {
    T v{};
    std::size_t len = sizeof v;
    if (::sysctlbyname(name, &v, &len, nullptr, 0) != 0) return std::nullopt;
    return v;
}
}  // namespace

std::string arch() {
    // A translated program is told the machine's architecture by asking
    // whether it is translated: the translation is to x86_64 from arm64.
    if (sysctl_value<int>("sysctl.proc_translated").value_or(0) == 1) return "aarch64";
    char m[64] = {};
    std::size_t len = sizeof m;
    if (::sysctlbyname("hw.machine", m, &len, nullptr, 0) != 0) return {};
    std::string s(m);
    return s == "arm64" ? "aarch64" : s;
}

std::optional<std::uint64_t> memory() { return sysctl_value<std::uint64_t>("hw.memsize"); }

}  // namespace mcpplibs::os::machine
