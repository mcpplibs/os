// Linux's own of mcpplibs.os.fs: an exchange (renameat2 RENAME_EXCHANGE) and
// the volume's kind (statfs), by the kernel's calls (src/linux/kernel.h).
module;
#include "kernel.h"

module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

namespace k = mcpplibs::os::linux_kernel;

int exchange(const std::filesystem::path& a, const std::filesystem::path& b) {
    return static_cast<int>(-k::renameat2(k::at_cwd, a.c_str(), k::at_cwd, b.c_str(), k::rename_exchange));
}
bool exchange_unsupported(int c) {
    return c == static_cast<int>(std::errc::invalid_argument) || c == static_cast<int>(std::errc::function_not_supported)
        || c == static_cast<int>(std::errc::operation_not_supported);
}

std::string volume_type(const std::filesystem::path& directory) {
    unsigned long long type = 0;
    if (k::statfs_type(directory.c_str(), &type) != 0) return {};
    switch (type) {
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
        default: return std::format("0x{:x}", type);
    }
}

}  // namespace mcpplibs::os::fs::sys
