module mcpplibs.os.linux;

import std;

namespace mcpplibs::os::linux {

namespace {
std::string read_small(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return s;
}
}  // namespace

std::optional<std::string> translator() {
    const std::string status = read_small("/proc/self/status");
    long tracer = 0;
    if (const auto at = status.find("TracerPid:"); at != std::string::npos)
        tracer = std::strtol(status.c_str() + at + 10, nullptr, 10);
    if (tracer > 0) {
        std::string comm = read_small(std::format("/proc/{}/comm", tracer).c_str());
        while (!comm.empty() && (comm.back() == '\n' || comm.back() == '\r')) comm.pop_back();
        if (comm.starts_with("proot")) return std::string("proot");
        return std::nullopt;
    }
    if (status.empty() && std::getenv("PROOT_TMP_DIR") != nullptr) return std::string("proot");
    return std::nullopt;
}

}  // namespace mcpplibs::os::linux
