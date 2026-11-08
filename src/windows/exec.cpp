// This system replaces no program's image: `exec' starts the program and ends
// as it ended (src/process.cpp).
module;
#include "win32.h"

module mcpplibs.os.process;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::process::sys {

result<void> replace_image(const std::filesystem::path&, const std::vector<std::string>&,
                           const std::vector<std::string>&, const std::filesystem::path&) {
    return std::unexpected(error{ errc::not_supported, "this system replaces no program's image" });
}

// This process's use, from the system's counters; a Windows program's children
// are not accounted together without a job object of their own.
result<usage> resources(of who) {
    if (who == of::children)
        return std::unexpected(error{ errc::not_supported, "Windows does not account a program's children together" });
    usage u;
    unsigned long long created = 0, exited = 0, kernel = 0, user = 0;
    if (late::GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        u.user = std::chrono::nanoseconds(user * 100);     // 100 ns units
        u.system = std::chrono::nanoseconds(kernel * 100);
    }
    okos_memory_counters m{};
    m.cb = sizeof m;
    if (late::K32GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof m)) u.peak_memory = m.peak_working_set;
    unsigned long long io[6] = {};
    if (late::GetProcessIoCounters(GetCurrentProcess(), &io)) {
        u.read_bytes = io[3];
        u.written_bytes = io[4];
    }
    return u;
}

}  // namespace mcpplibs::os::process::sys
