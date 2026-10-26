// macOS's own of mcpplibs.os.fs: an exchange (renamex_np RENAME_SWAP) and the
// volume's kind (statfs f_fstypename).
module;
#include <cerrno>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/param.h>

module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

int exchange(const std::filesystem::path& a, const std::filesystem::path& b) {
    return ::renamex_np(a.c_str(), b.c_str(), RENAME_SWAP) == 0 ? 0 : errno;
}
bool exchange_unsupported(int c) { return c == EINVAL || c == ENOTSUP || c == EOPNOTSUPP || c == ENOSYS; }

std::string volume_type(const std::filesystem::path& directory) {
    struct statfs st {};
    return ::statfs(directory.c_str(), &st) == 0 ? std::string(st.f_fstypename) : std::string{};
}

}  // namespace mcpplibs::os::fs::sys
