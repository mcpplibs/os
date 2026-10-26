module;
#include <sys/sysinfo.h>
#include <sys/utsname.h>

module mcpplibs.os.machine;

import std;

namespace mcpplibs::os::machine {

std::string arch() {
    utsname u{};
    if (::uname(&u) != 0) return {};
    std::string m = u.machine;
    if (m == "arm64") return "aarch64";
    return m;
}

std::optional<std::uint64_t> memory() {
    struct sysinfo s{};
    if (::sysinfo(&s) != 0) return std::nullopt;
    return static_cast<std::uint64_t>(s.totalram) * s.mem_unit;
}

}  // namespace mcpplibs::os::machine
