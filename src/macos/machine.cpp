module;
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

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

std::optional<std::uint64_t> memory_available() {
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (::host_statistics64(::mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count)
        != KERN_SUCCESS)
        return memory();
    const auto page = static_cast<std::uint64_t>(::getpagesize());
    return (static_cast<std::uint64_t>(vm.free_count) + static_cast<std::uint64_t>(vm.inactive_count)) * page;
}

processor_count processors() {
    processor_count c;
    const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (n > 0) c.logical = static_cast<std::uint32_t>(n);
    const int physical = sysctl_value<int>("hw.physicalcpu").value_or(0);
    c.physical = physical > 0 ? static_cast<std::uint32_t>(physical) : c.logical;
    c.heterogeneous = sysctl_value<int>("hw.nperflevels").value_or(1) > 1;
    return c;
}

}  // namespace mcpplibs::os::machine
