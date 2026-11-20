module;
#include "win32.h"

module mcpplibs.os.machine;

import std;

namespace mcpplibs::os::machine {

std::string arch() {
    unsigned short process = 0, native = 0;
    if (!late::IsWow64Process2(GetCurrentProcess(), &process, &native)) return {};
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
    if (!late::GlobalMemoryStatusEx(&s)) return std::nullopt;
    return s.total_physical;
}

std::optional<std::uint64_t> memory_available() {
    okos_memory_status s{};
    s.length = sizeof s;
    if (!late::GlobalMemoryStatusEx(&s)) return std::nullopt;
    return s.available_physical;
}

processor_count processors() {
    processor_count c;
    const auto n = late::GetActiveProcessorCount(0xffff /* ALL_PROCESSOR_GROUPS */);
    if (n > 0) c.logical = static_cast<std::uint32_t>(n);
    c.physical = c.logical;
    // One record per core (RelationProcessorCore): the count, and an
    // efficiency class that differs between cores of differing speed.
    okos_dword bytes = 0;
    late::GetLogicalProcessorInformationEx(0 /* RelationProcessorCore */, nullptr, &bytes);
    if (bytes == 0) return c;
    std::vector<unsigned char> buf(bytes);
    if (!late::GetLogicalProcessorInformationEx(0, buf.data(), &bytes)) return c;
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

// The registry's name of the first processor, as the system's own tools show
// it; advapi32 is reached at run time (src/windows/win32.h).
std::optional<std::string> processor_model() {
    using reg_get_value = int(__stdcall*)(void*, const okos_wchar*, const okos_wchar*, okos_dword, okos_dword*,
                                          void*, okos_dword*);
    const okos_handle advapi = LoadLibraryW(u"advapi32.dll");
    if (!advapi) return std::nullopt;
    const auto get = reinterpret_cast<reg_get_value>(GetProcAddress(advapi, "RegGetValueW"));
    if (!get) return std::nullopt;
    void* const local_machine = reinterpret_cast<void*>(static_cast<std::intptr_t>(static_cast<std::int32_t>(0x80000002)));
    okos_wchar value[128] = {};
    okos_dword bytes = sizeof value;
    if (get(local_machine, u"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", u"ProcessorNameString",
            0x2 /* RRF_RT_REG_SZ */, nullptr, value, &bytes) != 0)
        return std::nullopt;
    const std::u16string_view wide(value);
    const int n = WideCharToMultiByte(65001, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::nullopt;
    std::string name(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(65001, 0, wide.data(), static_cast<int>(wide.size()), name.data(), n, nullptr, nullptr);
    const auto begin = name.find_first_not_of(' ');
    if (begin == std::string::npos) return std::nullopt;
    return name.substr(begin, name.find_last_not_of(' ') - begin + 1);
}

}  // namespace mcpplibs::os::machine
