// The Linux kernel's own calls, for what this package's Linux units ask the
// system beside openkal: the machine's name, its memory, a volume's kind and
// an exchange of two entries.
//
// THE KERNEL AND NOT THE C LIBRARY. A program over this package chooses its C
// library, and that library may sit on openkal rather than on the kernel
// (openkal-musl, mcpp docs/24): its `uname' then names openkal, its `sysinfo'
// is not provided, and its `syscall' reaches openkal's port, not the kernel.
// The kernel's interface is the system's stable one and is the same under
// every C library, so the answers below are the system's in every
// arrangement. One file per architecture (src/linux/arch/) holds the trap and
// the numbers; the manifest selects it.
#pragma once

namespace mcpplibs::os::linux_kernel {

// `struct new_utsname'.
struct utsname_t {
    char sysname[65], nodename[65], release[65], version[65], machine[65], domainname[65];
};

// `struct sysinfo' of a 64-bit kernel: every field a 64-bit kernel `long' or
// as written, 112 bytes; the tail is room the kernel never fills.
struct sysinfo_t {
    long long          uptime;
    unsigned long long loads[3];
    unsigned long long totalram, freeram, sharedram, bufferram, totalswap, freeswap;
    unsigned short     procs, pad;
    unsigned long long totalhigh, freehigh;
    unsigned int       mem_unit;
    char               reserved[64];
};

inline constexpr int      at_cwd          = -100;   // AT_FDCWD
inline constexpr unsigned rename_exchange = 2;      // RENAME_EXCHANGE

// Each returns 0, or the negated error number, as the kernel does; the error
// numbers are the kernel's, which every C library on Linux uses.
long uname(utsname_t* out);
long sysinfo(sysinfo_t* out);
// The kind (`f_type') of the volume that holds `path'.
long statfs_type(const char* path, unsigned long long* type);
long renameat2(int from_dir, const char* from, int to_dir, const char* to, unsigned flags);

// inotify (0.4.2, `fs::watcher'): an instance that does not block a read and is
// closed on exec, the watch of a node and its removal, its records read, and a
// wait for it to be readable of at most `milliseconds' (negative: no bound).
inline constexpr unsigned in_modify = 0x2, in_attrib = 0x4, in_close_write = 0x8, in_moved_from = 0x40,
                          in_moved_to = 0x80, in_create = 0x100, in_delete = 0x200, in_delete_self = 0x400,
                          in_move_self = 0x800, in_q_overflow = 0x4000, in_ignored = 0x8000, in_onlydir = 0x1000000;
long inotify_open();
long inotify_watch(int fd, const char* path, unsigned mask);
long inotify_unwatch(int fd, int watch);
long read(int fd, void* bytes, unsigned long count);
long wait_readable(int fd, long milliseconds);

}  // namespace mcpplibs::os::linux_kernel
