module;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

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

namespace {
bool meminfo(const char* key, std::uint64_t& out) {
    FILE* f = ::fopen("/proc/meminfo", "r");
    if (!f) return false;
    char line[256];
    bool found = false;
    const std::size_t klen = ::strlen(key);
    while (::fgets(line, sizeof line, f)) {
        if (::strncmp(line, key, klen) != 0) continue;
        unsigned long long kb = 0;
        if (::sscanf(line + klen, " %llu", &kb) == 1) { out = kb * 1024ull; found = true; }
        break;
    }
    ::fclose(f);
    return found;
}
int cores_per_package() {
    FILE* f = ::fopen("/proc/cpuinfo", "r");
    if (!f) return 0;
    char line[256];
    int cores = 0;
    while (::fgets(line, sizeof line, f)) {
        if (::strncmp(line, "cpu cores", 9) != 0) continue;
        if (const char* colon = ::strchr(line, ':')) cores = ::atoi(colon + 1);
        break;
    }
    ::fclose(f);
    return cores;
}
// Cores whose top frequencies differ: performance and efficiency cores.
bool hybrid(long logical) {
    if (logical <= 1) return false;
    long first = -1;
    for (long i = 0; i < logical; ++i) {
        char path[128];
        ::snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%ld/cpufreq/cpuinfo_max_freq", i);
        FILE* f = ::fopen(path, "r");
        if (!f) return false;
        long v = 0;
        const int got = ::fscanf(f, "%ld", &v);
        ::fclose(f);
        if (got != 1) return false;
        if (first < 0) first = v;
        else if (v != first) return true;
    }
    return false;
}
}  // namespace

std::optional<std::uint64_t> memory_available() {
    std::uint64_t v = 0;
    if (meminfo("MemAvailable:", v)) return v;
    return memory();
}

processor_count processors() {
    processor_count c;
    const long n = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (n > 0) c.logical = static_cast<std::uint32_t>(n);
    const int cores = cores_per_package();
    c.physical = cores > 0 ? static_cast<std::uint32_t>(cores) : c.logical;
    c.heterogeneous = hybrid(static_cast<long>(c.logical));
    return c;
}

}  // namespace mcpplibs::os::machine
