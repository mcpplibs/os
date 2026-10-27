// mcpplibs.os.machine on macOS: sysctl and the host's statistics, through
// libSystem (src/macos/darwin.h).
module;
#include "darwin.h"

module mcpplibs.os.machine;

import std;

namespace mcpplibs::os::machine {

namespace {
template <class T>
std::optional<T> sysctl_value(const char* name) {
    T v{};
    unsigned long len = sizeof v;
    if (::sysctlbyname(name, &v, &len, nullptr, 0) != 0) return std::nullopt;
    return v;
}
}  // namespace

std::string arch() {
    // A translated program is told the machine's architecture by asking
    // whether it is translated: the translation is to x86_64 from arm64.
    if (sysctl_value<int>("sysctl.proc_translated").value_or(0) == 1) return "aarch64";
    char m[64] = {};
    unsigned long len = sizeof m;
    if (::sysctlbyname("hw.machine", m, &len, nullptr, 0) != 0) return {};
    std::string s(m);
    return s == "arm64" ? "aarch64" : s;
}

std::optional<std::uint64_t> memory() { return sysctl_value<std::uint64_t>("hw.memsize"); }

std::optional<std::uint64_t> memory_available() {
    const unsigned int host = ::mach_host_self();
    okos_vm_statistics64 vm{};
    unsigned int count = sizeof vm / sizeof(int);
    unsigned long page = 0;
    if (::host_statistics64(host, 4 /* HOST_VM_INFO64 */, reinterpret_cast<int*>(&vm), &count) != 0
        || ::host_page_size(host, &page) != 0 || page == 0)
        return memory();
    return (static_cast<std::uint64_t>(vm.free_count) + static_cast<std::uint64_t>(vm.inactive_count)) * page;
}

processor_count processors() {
    processor_count c;
    c.logical = static_cast<std::uint32_t>(std::max(1, sysctl_value<int>("hw.activecpu").value_or(1)));
    const int physical = sysctl_value<int>("hw.physicalcpu").value_or(0);
    c.physical = physical > 0 ? std::min(static_cast<std::uint32_t>(physical), c.logical) : c.logical;
    c.heterogeneous = sysctl_value<int>("hw.nperflevels").value_or(1) > 1;
    return c;
}

}  // namespace mcpplibs::os::machine
