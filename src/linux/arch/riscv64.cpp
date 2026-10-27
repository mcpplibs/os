// The kernel's calls on riscv64 Linux (src/posix/kernel.h, src/linux/kernel.h):
// the trap, and the numbers of the calls this package makes, from the kernel's
// include/uapi/asm-generic/unistd.h.
#include "../kernel.h"
#include "../../posix/kernel.h"

namespace {

long call(long n, long a = 0, long b = 0, long c = 0, long d = 0, long e = 0) {
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c;
    register long a3 __asm__("a3") = d;
    register long a4 __asm__("a4") = e;
    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(a7), "r"(a1), "r"(a2), "r"(a3), "r"(a4) : "memory");
    return a0;
}

constexpr long nr_write = 64,
               nr_close = 57,
               nr_fcntl = 25,
               nr_dup3 = 24,
               nr_rt_sigaction = 134,
               nr_rt_sigprocmask = 135,
               nr_getpid = 172,
               nr_kill = 129,
               nr_uname = 160,
               nr_sysinfo = 179,
               nr_statfs = 43,
               nr_renameat2 = 276;

constexpr long f_dupfd_cloexec = 1030, sig_unblock = 1;

}  // namespace

namespace mcpplibs::os::kernel {

long dup_above(int fd, int lowest) { return call(nr_fcntl, fd, f_dupfd_cloexec, lowest); }

long dup_onto(int from, int to) {
    if (from == to) return call(nr_fcntl, from, 1 /* F_GETFD */) < 0 ? -9 /* EBADF */ : to;
    return call(nr_dup3, from, to, 0);
}

long write(int fd, const void* bytes, unsigned long count) {
    return call(nr_write, fd, reinterpret_cast<long>(bytes), static_cast<long>(count));
}

long close(int fd) { return call(nr_close, fd); }

void default_signal(int signal) {
    // The kernel's `struct sigaction', zero: SIG_DFL, no flags, nothing masked.
    alignas(8) unsigned long long action[4] = {};
    call(nr_rt_sigaction, signal, reinterpret_cast<long>(action), 0, 8);
}

void raise_signal(int signal) {
    unsigned long long set = 1ull << (signal - 1);
    call(nr_rt_sigprocmask, sig_unblock, reinterpret_cast<long>(&set), 0, 8);
    call(nr_kill, call(nr_getpid), signal);
}

}  // namespace mcpplibs::os::kernel

namespace mcpplibs::os::linux_kernel {

long uname(utsname_t* out) { return call(nr_uname, reinterpret_cast<long>(out)); }

long sysinfo(sysinfo_t* out) { return call(nr_sysinfo, reinterpret_cast<long>(out)); }

long statfs_type(const char* path, unsigned long long* type) {
    // `struct statfs' of a 64-bit kernel begins with `f_type'; 120 bytes, here 256.
    alignas(8) unsigned long long st[32] = {};
    const long r = call(nr_statfs, reinterpret_cast<long>(path), reinterpret_cast<long>(st));
    if (r == 0) *type = st[0];
    return r;
}

long renameat2(int from_dir, const char* from, int to_dir, const char* to, unsigned flags) {
    return call(nr_renameat2, from_dir, reinterpret_cast<long>(from), to_dir, reinterpret_cast<long>(to),
                static_cast<long>(flags));
}

}  // namespace mcpplibs::os::linux_kernel
