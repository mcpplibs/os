module;
#include "win32.h"

module mcpplibs.os.machine;

import std;

namespace mcpplibs::os::machine {

std::string arch() {
    unsigned short process = 0, native = 0;
    if (!IsWow64Process2(GetCurrentProcess(), &process, &native)) return {};
    switch (native) {
        case 0x8664: return "x86_64";     // IMAGE_FILE_MACHINE_AMD64
        case 0xAA64: return "aarch64";    // IMAGE_FILE_MACHINE_ARM64
        case 0x014c: return "x86";        // IMAGE_FILE_MACHINE_I386
        default:     return std::format("machine-{:04x}", native);
    }
}

std::optional<std::uint64_t> memory() {
    okos_memory_status s{};
    s.length = sizeof s;
    if (!GlobalMemoryStatusEx(&s)) return std::nullopt;
    return s.total_physical;
}

std::optional<std::uint64_t> memory_available() {
    okos_memory_status s{};
    s.length = sizeof s;
    if (!GlobalMemoryStatusEx(&s)) return std::nullopt;
    return s.available_physical;
}

processor_count processors() {
    processor_count c;
    const auto n = GetActiveProcessorCount(0xffff /* ALL_PROCESSOR_GROUPS */);
    if (n > 0) c.logical = static_cast<std::uint32_t>(n);
    c.physical = c.logical;
    // One record per core (RelationProcessorCore): the count, and an
    // efficiency class that differs between cores of differing speed.
    okos_dword bytes = 0;
    GetLogicalProcessorInformationEx(0 /* RelationProcessorCore */, nullptr, &bytes);
    if (bytes == 0) return c;
    std::vector<unsigned char> buf(bytes);
    if (!GetLogicalProcessorInformationEx(0, buf.data(), &bytes)) return c;
    std::uint32_t count = 0;
    int first = -1;
    for (okos_dword off = 0; off + 8 < bytes;) {
        // SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX: Relationship (4), Size (4),
        // then PROCESSOR_RELATIONSHIP: Flags (1), EfficiencyClass (1), ...
        const auto size = *reinterpret_cast<const okos_dword*>(buf.data() + off + 4);
        if (size == 0) break;
        const int cls = buf[off + 9];
        ++count;
        if (first < 0) first = cls;
        else if (cls != first) c.heterogeneous = true;
        off += size;
    }
    if (count > 0) c.physical = count;
    return c;
}

}  // namespace mcpplibs::os::machine
