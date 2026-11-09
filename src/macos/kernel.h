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

// kqueue (0.4.2, `fs::watcher'): `struct kevent' (sys/event.h), a node's
// notices (EVFILT_VNODE), a queue, its changes and events with a wait of at
// most `milliseconds' (negative: no bound), and a descriptor for notices only.
struct kevent_t {
    unsigned long  ident;
    short          filter;
    unsigned short flags;
    unsigned int   fflags;
    long           data;
    void*          udata;
};
inline constexpr short evfilt_vnode = -4;
inline constexpr unsigned short ev_add = 0x1, ev_delete = 0x2, ev_enable = 0x4, ev_clear = 0x20;
inline constexpr unsigned note_delete = 0x1, note_write = 0x2, note_extend = 0x4, note_attrib = 0x8, note_link = 0x10,
                          note_rename = 0x20, note_revoke = 0x40;
long kqueue();
long kevent(int kq, const kevent_t* changes, int count, kevent_t* events, int capacity, long milliseconds);
long open_events(const char* path);

}  // namespace mcpplibs::os::macos_kernel
