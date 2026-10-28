// mcpplibs.os.machine on Linux: the kernel's calls for the machine's name and
// memory (src/linux/kernel.h), and the kernel's files for the rest. A file is
// read through the program's C library, which reads it the same over every
// kernel layer.
module;
#include "kernel.h"

module mcpplibs.os.machine;

import std;

namespace mcpplibs::os::machine {

namespace k = mcpplibs::os::linux_kernel;

namespace {

// The first line of a kernel file, or nothing.
std::string first_line(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::string line;
    std::getline(in, line);
    return line;
}

// The value after `key' in /proc/meminfo, in bytes.
std::optional<std::uint64_t> meminfo(std::string_view key) {
    std::ifstream in("/proc/meminfo", std::ios::binary);
    for (std::string line; std::getline(in, line);) {
        if (!line.starts_with(key)) continue;
        std::size_t i = key.size();
        while (i < line.size() && line[i] == ' ') ++i;
        std::uint64_t kb = 0;
        const auto [end, ec] = std::from_chars(line.data() + i, line.data() + line.size(), kb);
        if (ec != std::errc{}) return std::nullopt;
        return kb * 1024u;
    }
    return std::nullopt;
}

// The processors online: a list of ranges, `0-3,8-11'.
std::uint32_t online(std::string_view list) {
    std::uint32_t n = 0;
    while (!list.empty()) {
        const auto comma = list.find(',');
        const auto part = list.substr(0, comma);
        unsigned lo = 0, hi = 0;
        const auto dash = part.find('-');
        const auto [e1, c1] = std::from_chars(part.data(), part.data() + std::min(dash, part.size()), lo);
        hi = lo;
        if (dash != std::string_view::npos) std::from_chars(part.data() + dash + 1, part.data() + part.size(), hi);
        if (c1 == std::errc{} && hi >= lo) n += hi - lo + 1;
        if (comma == std::string_view::npos) break;
        list.remove_prefix(comma + 1);
    }
    return n;
}

int cores_per_package() {
    std::ifstream in("/proc/cpuinfo", std::ios::binary);
    for (std::string line; std::getline(in, line);) {
        if (!line.starts_with("cpu cores")) continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos) return 0;
        std::size_t i = colon + 1;
        while (i < line.size() && line[i] == ' ') ++i;
        int cores = 0;
        std::from_chars(line.data() + i, line.data() + line.size(), cores);
        return cores;
    }
    return 0;
}

// Cores whose top frequencies differ: performance and efficiency cores.
bool hybrid(std::uint32_t logical) {
    if (logical <= 1) return false;
    std::string first;
    for (std::uint32_t i = 0; i < logical; ++i) {
        const auto v = first_line(std::format("/sys/devices/system/cpu/cpu{}/cpufreq/cpuinfo_max_freq", i).c_str());
        if (v.empty()) return false;
        if (first.empty()) first = v;
        else if (v != first) return true;
    }
    return false;
}

}  // namespace

std::string arch() {
    k::utsname_t u{};
    if (k::uname(&u) != 0) return {};
    std::string m = u.machine;
    if (m == "arm64") return "aarch64";
    return m;
}

std::optional<std::uint64_t> memory() {
    k::sysinfo_t s{};
    if (k::sysinfo(&s) != 0) return std::nullopt;
    return s.totalram * (s.mem_unit ? s.mem_unit : 1u);
}

std::optional<std::uint64_t> memory_available() {
    if (const auto v = meminfo("MemAvailable:")) return v;
    return memory();
}

processor_count processors() {
    processor_count c;
    c.logical = online(first_line("/sys/devices/system/cpu/online"));
    if (c.logical == 0) c.logical = std::max(1u, std::thread::hardware_concurrency());
    const int cores = cores_per_package();
    c.physical = cores > 0 ? std::min(static_cast<std::uint32_t>(cores), c.logical) : c.logical;
    c.heterogeneous = hybrid(c.logical);
    return c;
}

}  // namespace mcpplibs::os::machine
