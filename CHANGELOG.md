# Changelog

## 0.2.1

- **The ok-image targets take their C library, C++ run-time and loader from
  their platform.** `[target.'cfg(os = "openkal")']` no longer declares
  openkal-llvm-runtime or openkal-tools: mcpp 2026.10.27.1 selects
  openkal-platform for `<arch>-openkal` (mcpp SPEC-013). CI finds the runner
  through `mcpp toolchain install --target` and `mcpp self env --target`.

The package builds and its tests pass in every arrangement of the program's C
library on Linux, macOS and Windows: the system's own (glibc, libSystem, the
Microsoft C runtime), and a C library that sits on openkal (openkal-musl with
openkal-llvm-runtime, mcpp docs/24) on the system's own triple. Under the
second, a C library presents POSIX and openkal, not the system: its headers are
not the SDK's, on Windows its data model is LP64 and its `wchar_t` 32 bits, and
its descriptors, signals, `uname`, `sysinfo` and `syscall` are its own.

- **The units beside openkal reach each system through that system's stable
  interface, not through the C library.**
  - **Linux:** the kernel's calls, one trap per architecture
    (`src/linux/kernel.h`, `src/linux/arch/`). These cover the machine's name
    and memory, a volume's kind and an exchange of two entries. In the second
    arrangement the C library had answered `openkal` for the machine, had no
    `sysinfo`, and turned `renameat2(RENAME_EXCHANGE)` into a plain rename that
    replaced the second entry.
  - **macOS:** the kernel's calls for a volume's kind and an exchange
    (`src/macos/kernel.h`, as openkal-macos itself does), and the libSystem
    names no C library defines for sysctl and the host's statistics
    (`src/macos/darwin.h`). These are declared rather than included from the
    SDK. A link whose libSystem is openkal-macos's stub binds them when the
    program is loaded (`-U`).
  - **Windows:** `src/windows/win32.h` declares DWORD as 32 bits and WCHAR as
    UTF-16 (`char16_t`) under every C environment. Names reach the system as
    UTF-8 converted to UTF-16, never through `path::c_str()` (R7). The
    kernel32 names that openkal-windows does not itself call are found when
    first called, so a toolchain without the system's import libraries links
    the package.
- **The process's output and its ending for a closed reader are changed where
  the process keeps them.**
  - On Linux and macOS, the diversion of the standard output (`output_to_error`),
    the reset of SIGPIPE before `exec` and `end_as_output_closed` use the
    kernel's descriptors and dispositions (`src/posix/kernel.h`): what openkal's
    standard output, the C library's and a started program all use.
  - On Windows the diversion moves the Microsoft C runtime's descriptor, as
    before (`src/windows/crt/stdio.cpp`). For a C library on openkal it moves
    that library's descriptor and the process's standard handle, each apart
    (`src/windows/posix/stdio.cpp`). The manifest selects the form by
    `cfg(c-abi = "musl")`.
- **`checks/hosted`** builds the package's tests over openkal-musl and
  openkal-llvm-runtime. CI runs them on Linux, macOS and Windows, beside the
  native rows and the ok images. Locally, the native and hosted suites pass on
  Linux x86_64 (gcc 16.1.0 and llvm 23.1.3, dev and release) and on aarch64
  under qemu. The Windows and macOS builds are cross-linked from Linux.
- **What mcpp-language-server's operating-system layer carried** (its
  `modules/os` and `modules/platform`, removed in its 0.0.18):
  - `process::run(options, bounds, input)`: a run with a hard bound, a grace
    and a drain, and limits on what each output keeps. It reports a timeout,
    an output a descendant held open, and an output it cut.
  - `process::threads(id)`: the threads of a program and the processor time of
    each (Linux; none elsewhere).
  - `env::assignments(entries)`: `NAME=value` entries as the overrides of a
    started program's environment.
  - `fs::read(file, at_most)`: a bounded read, `errc::too_large` past the
    bound (new error condition).
  - `fs::canonical(name)`: the name a node is known by, with links followed
    and, on Windows, short (8.3) components replaced by their long names.
- Tests: `platform_test` asserts that Linux names the machine as the processor
  the program runs on. `fs_test` asserts that a volume's kind is named.
  `process_test` makes a file executable with `os::fs::set_executable` rather
  than with `std::filesystem::permissions`, which a C library on openkal
  refuses. `process_test` covers the bounded run (a cut output, a hard bound,
  an output held open), the threads of this program and environment entries;
  `fs_test` covers the bounded read and the canonical name.

## 0.2.0

What mcpp's operating-system layer carried: `exec`; both outputs on one pipe;
appending and durability; measured volumes; place and exchange; owners,
processors and available memory; a bounded read of input; output sent to the
error stream; raw mode that keeps the interrupt; Windows facts. Over openkal
0.17.1 (openkal-linux 0.18.2, openkal-macos 0.15.1, openkal-windows 0.13.3); ok
images over openkal-llvm-runtime 0.16.3 and openkal-tools 0.1.1 (#3).

## 0.1.2

Shared libraries at run time (`mcpplibs.os.library`) (#5).

## 0.1.1

Names reach the system as UTF-8 and come back as UTF-8 on every host, never
through the process's code page (R7) (#4).

## 0.1.0

A C++ operating-system library over openkal 0.17 (#1, #2).
