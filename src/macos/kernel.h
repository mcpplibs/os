// macOS's kernel calls for what this package's macOS units ask beside openkal:
// a volume's kind and an exchange of two entries, the calls openkal-macos
// itself issues for its own questions about volumes and renames. Defined in
// src/macos/arch/ with the calls of src/posix/kernel.h.
#pragma once

namespace mcpplibs::os::macos_kernel {

// `struct statfs64' (sys/mount.h, 64-bit inodes).
struct statfs_t {
    unsigned int       bsize;
    int                iosize;
    unsigned long long blocks, bfree, bavail, files, ffree;
    int                fsid[2];
    unsigned int       owner, type, flags, fssubtype;
    char               fstypename[16];
    char               mntonname[1024];
    char               mntfromname[1024];
    unsigned int       flags_ext;
    unsigned int       reserved[7];
};

inline constexpr int      at_cwd      = -2;    // AT_FDCWD
inline constexpr unsigned rename_swap = 0x2;   // RENAME_SWAP

// Each returns 0, or the negated error number in this system's numbering.
long statfs(const char* path, statfs_t* out);
long renameatx_np(int from_dir, const char* from, int to_dir, const char* to, unsigned flags);

}  // namespace mcpplibs::os::macos_kernel
