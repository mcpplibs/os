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

}  // namespace mcpplibs::os::machine
