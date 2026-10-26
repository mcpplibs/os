// Linux's own of mcpplibs.os.fs: an exchange (renameat2 RENAME_EXCHANGE) and
// the volume's kind (statfs).
module;
#include <cerrno>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>

module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

int exchange(const std::filesystem::path& a, const std::filesystem::path& b) {
    constexpr unsigned kExchange = 2;   // RENAME_EXCHANGE
    return ::syscall(SYS_renameat2, AT_FDCWD, a.c_str(), AT_FDCWD, b.c_str(), kExchange) == 0 ? 0 : errno;
}
bool exchange_unsupported(int c) { return c == EINVAL || c == ENOSYS || c == EOPNOTSUPP; }

std::string volume_type(const std::filesystem::path& directory) {
    struct statfs st {};
    if (::statfs(directory.c_str(), &st) != 0) return {};
    switch (static_cast<unsigned long>(st.f_type)) {
        case 0xEF53: return "ext4";
        case 0x58465342: return "xfs";
        case 0x9123683E: return "btrfs";
        case 0x01021994: return "tmpfs";
        case 0x794C7630: return "overlay";
        case 0x6969: return "nfs";
        case 0xFF534D42: return "cifs";
        case 0xFE534D42: return "smb2";
        case 0x4D44: return "vfat";
        case 0x2011BAB0: return "exfat";
        case 0x7366746E: return "ntfs3";
        case 0x5346544E: return "ntfs";
        case 0x65735546: return "fuse";
        case 0x01021997: return "9p";
        case 0x2FC12FC1: return "zfs";
        case 0x00C36400: return "ceph";
        default: return std::format("0x{:x}", static_cast<unsigned long>(st.f_type));
    }
}

}  // namespace mcpplibs::os::fs::sys
