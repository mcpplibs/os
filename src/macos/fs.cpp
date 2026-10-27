// macOS's own of mcpplibs.os.fs: an exchange (renameatx_np RENAME_SWAP) and the
// volume's kind (statfs64 f_fstypename), by the kernel's calls
// (src/macos/kernel.h).
module;
#include "kernel.h"

module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

namespace k = mcpplibs::os::macos_kernel;

namespace {

// This system's error number as the program's C library numbers it. The first
// thirty-four agree everywhere; the others this unit can meet are named.
int condition_of(long darwin) {
    if (darwin <= 34) return static_cast<int>(darwin);
    switch (darwin) {
        case 45:  return static_cast<int>(std::errc::not_supported);            // ENOTSUP
        case 63:  return static_cast<int>(std::errc::filename_too_long);        // ENAMETOOLONG
        case 66:  return static_cast<int>(std::errc::directory_not_empty);      // ENOTEMPTY
        case 78:  return static_cast<int>(std::errc::function_not_supported);   // ENOSYS
        case 102: return static_cast<int>(std::errc::operation_not_supported);  // EOPNOTSUPP
        default:  return static_cast<int>(std::errc::io_error);
    }
}

}  // namespace

int exchange(const std::filesystem::path& a, const std::filesystem::path& b) {
    const long r = k::renameatx_np(k::at_cwd, a.c_str(), k::at_cwd, b.c_str(), k::rename_swap);
    return r == 0 ? 0 : condition_of(-r);
}
bool exchange_unsupported(int c) {
    return c == static_cast<int>(std::errc::invalid_argument) || c == static_cast<int>(std::errc::not_supported)
        || c == static_cast<int>(std::errc::operation_not_supported)
        || c == static_cast<int>(std::errc::function_not_supported);
}

std::string volume_type(const std::filesystem::path& directory) {
    k::statfs_t st{};
    if (k::statfs(directory.c_str(), &st) != 0) return {};
    const std::string_view name(st.fstypename, sizeof st.fstypename);
    return std::string(name.substr(0, name.find(char{})));
}

}  // namespace mcpplibs::os::fs::sys
