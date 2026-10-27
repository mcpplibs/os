// The kernel's calls on x86_64 macOS (src/posix/kernel.h, src/macos/kernel.h): the trap, and the
// numbers of the calls this package makes, from the kernel's
// bsd/kern/syscalls.master --- the same on both architectures, and the ones
// openkal-macos issues.
#include "../kernel.h"
#include "../../posix/kernel.h"

namespace {

long call(long n, long a = 0, long b = 0, long c = 0, long d = 0, long e = 0) {
    long r;
    unsigned char failed;
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    // The class of the ordinary calls (2) in the number's high bits; the
    // carry flag reports a failure and the result is then the error number.
    __asm__ __volatile__("syscall"
                         : "=a"(r), "+d"(c), "=@ccc"(failed)
                         : "a"(n | 0x2000000L), "D"(a), "S"(b), "r"(r10), "r"(r8)
                         : "rcx", "r11", "memory", "cc");
    return failed ? -r : r;
}

constexpr long nr_write = 4, nr_close = 6, nr_getpid = 20, nr_kill = 37, nr_sigaction = 46,
               nr_sigprocmask = 48, nr_dup2 = 90, nr_fcntl = 92, nr_statfs64 = 345, nr_renameatx_np = 488;

constexpr long f_dupfd_cloexec = 67, sig_unblock = 2;

}  // namespace

namespace mcpplibs::os::kernel {

long dup_above(int fd, int lowest) { return call(nr_fcntl, fd, f_dupfd_cloexec, lowest); }

long dup_onto(int from, int to) { return call(nr_dup2, from, to); }

long write(int fd, const void* bytes, unsigned long count) {
    return call(nr_write, fd, reinterpret_cast<long>(bytes), static_cast<long>(count));
}

long close(int fd) { return call(nr_close, fd); }

void default_signal(int signal) {
    // The kernel's `struct __sigaction' (handler, trampoline, mask, flags),
    // zero: SIG_DFL, which the kernel enters no trampoline for.
    struct { void* handler; void* trampoline; unsigned int mask; int flags; } action{};
    call(nr_sigaction, signal, reinterpret_cast<long>(&action), 0);
}

void raise_signal(int signal) {
    unsigned int set = 1u << (signal - 1);
    call(nr_sigprocmask, sig_unblock, reinterpret_cast<long>(&set), 0);
    call(nr_kill, call(nr_getpid), signal, 1 /* POSIX */);
}

}  // namespace mcpplibs::os::kernel

namespace mcpplibs::os::macos_kernel {

long statfs(const char* path, statfs_t* out) {
    return call(nr_statfs64, reinterpret_cast<long>(path), reinterpret_cast<long>(out));
}

long renameatx_np(int from_dir, const char* from, int to_dir, const char* to, unsigned flags) {
    return call(nr_renameatx_np, from_dir, reinterpret_cast<long>(from), to_dir, reinterpret_cast<long>(to),
                static_cast<long>(flags));
}

}  // namespace mcpplibs::os::macos_kernel
