// The descriptors and the signals of the process, by the kernel's own calls:
// what the units in src/posix/ do to the process rather than to the program's
// C library.
//
// A C library keeps descriptors and dispositions of its own when it sits on
// openkal rather than on the kernel (openkal-musl, mcpp docs/24): `dup2' then
// moves the library's descriptor and not the one openkal's standard streams
// and a started program use, and `signal' changes nothing the kernel acts on.
// Diverting the output and ending as a closed reader ends are acts upon the
// process, so they are made where the process is: in the kernel. Under the
// system's own C library the two coincide, and the result is the same.
//
// Each returns what the call returned, or the negated error number; the
// numbers are the system's (EINTR is 4 on both). Defined per system and
// architecture in src/linux/arch/ and src/macos/arch/.
#pragma once

namespace mcpplibs::os::kernel {

// A new descriptor for `fd', the lowest not below `lowest', closed on exec.
long dup_above(int fd, int lowest);
// `to' made a descriptor for what `from' is, closed first if open.
long dup_onto(int from, int to);
long write(int fd, const void* bytes, unsigned long count);
long close(int fd);
// The signal at its default disposition.
void default_signal(int signal);
// The signal unblocked in this thread and sent to this process.
void raise_signal(int signal);

inline constexpr int sigpipe = 13;   // the same number on Linux and macOS

}  // namespace mcpplibs::os::kernel
