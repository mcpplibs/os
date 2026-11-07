# Changelog

## 0.4.0

- **`is_openkal`, named as its platform is.** A build for the openkal target
  (`<arch>-openkal`, mcpp's `cfg(os = "openkal")`), whose artifact is an ok
  image, is `is_openkal`, as a build for Linux is `is_linux`: each
  `is_<platform>` names the target of the build, and `platform::openkal` is
  this one's. `is_image`, its name since 0.1.0, read as though it named a
  container's or a disk's image, and as though only it were over openkal, which
  every build of this package is. It remains, deprecated, as the same constant.
- **A platform question for a native build and an image alike.** `can_run_on(p)`
  says whether this build can run on `p` (a native build on its own system, an
  image on every system a loader runs upon), and `on<P>(f)` runs `f` when the
  build runs on `P` now. `if constexpr (can_run_on(P))` keeps another system's
  platform module out of a native build, as `if constexpr (is_<platform>)` does;
  `host()` then decides in an image, where `is_<platform>` is false and a branch
  under it never runs. `on<P>` takes the platform as an argument, so its body is
  a template a build that cannot run on `P` never instantiates; a lambda without
  the parameter is refused. `is_<platform>` keeps its meaning: a native build of
  that platform (README, "A platform question").
- **The platform modules in an image (R5 as revised).** A platform module is
  defined where it can be answered: in a native build of its platform, and in an
  image where the image can ask the system it runs upon. `linux::translator()`
  reads /proc as on Linux; `windows::elevated()` reads the integrity level
  `whoami /groups` lists, by SID; `windows::on_dev_drive()` asks `fsutil devdrv
  query`; `posix::effective_user()` reads /proc on Linux and `/usr/bin/id -u` on
  macOS; `macos::translated()` is `false`, since macOS runs images through an
  aarch64 okrun only. The system's programs are started by absolute name with a
  bound of ten seconds, and each answer is asked once. The code pages, long
  names, the registry, the writable drives and the creation mask remain
  undefined in an image: okrun carries an image's streams and names, and the
  others belong to a native installation. Before 0.4.0 an image defined no
  platform module, and a program shipped as an image that guarded them with
  `if constexpr (is_<platform>)` lost them without a word (mcpp-community/mcpp
  design 2026-11-08, S2 and S3).
- **The user and the creation mask are the kernel's.** On Linux and macOS
  `posix::effective_user()` and `posix::set_creation_mask()` make the kernel's
  own calls (src/posix/kernel.h), as the descriptors and the signals already
  did. In the hosted arrangement the C library is openkal-musl, which sits on
  openkal: openkal names no user, so its `geteuid` answers the constant 1000,
  and it has no `umask`. The answer was then 1000 for every user, and a mask
  was set nowhere; it is now the process's under every C library. Measured on
  the CI runners (user 1001) and under `unshare -r` (user 0).

## 0.3.1

- **A name in the long spelling is the name it spells.** `fs::extended` writes
  `\\?\C:\...` and `\\?\UNC\server\share\...` on Windows. `locate` compared that
  spelling as written: the leading `//` matched the supplied directory of shares
  and the remainder kept `?/C:`, whose colon openkal refuses, so `read`, `write`,
  `rename`, `stamp_of`, `identity_of` and every other located operation upon such a
  name failed with `errc::invalid` (mcpp-community/mcpp#889). The long spelling is
  now read as the plain name it denotes, before the name is normalized (a C++
  library with POSIX's rules reads the leading `//` as `/`). `fs::extended` makes a
  name absolute by this library's own reading of a Windows name: over the hosted
  arrangement `std::filesystem::absolute` took `C:\Users\...` for a relative name
  and put the working directory before it. The Windows test writes, reads,
  stamps, identifies and renames a file through both spellings, natively and over
  the hosted arrangement (openkal-platform 0.24.2, whose C++ runtime keeps the
  volume of a normalized name, openkal-llvm-runtime#68).
- **The kernel layer is a compatible range.** openkal `^0.24.0` and its
  implementations `^0.25.0` (Linux), `^0.22.0` (macOS), `^0.20.0` (Windows) and
  `^0.13.0` (Emscripten). A target that selects openkal-platform receives the
  versions of its version group; any other build receives the newest compatible
  releases, recorded in `mcpp.lock`.

## 0.2.7

- **openkal 0.21.** The implementations of openkal 0.21 (openkal-linux 0.22.0,
  openkal-macos 0.19.0, openkal-windows 0.17.0, openkal-emscripten 0.10.0). This
  library's interface is unchanged. An ok image over this release selects
  openkal-platform 0.21.0, whose loader maps an image's instructions from its
  file and whose C library's allocator is mimalloc; `checks/hosted` and the
  image checks take theirs from it, built with mcpp 2026.11.2.1.

## 0.2.6

- **openkal-macos 0.17.1**, whose `kal_task_current` reads the thread's
  identifier from the record the system's library keeps instead of a system
  call per answer. This library's interface is unchanged; an ok image over this
  release selects openkal-platform 0.20.1, which `checks/hosted` and the image
  checks take.

## 0.2.5

- **openkal 0.19.** The implementations of openkal 0.19 (openkal-linux 0.20.0,
  openkal-macos 0.17.0, openkal-windows 0.15.0, openkal-emscripten 0.8.0), which
  state the physical cores and set a context's claim on the processors
  (`kal_task_cores`, `kal_task_set_priority`). This library's own interface is
  unchanged: no program above it needs either yet, and os grows by its
  consumers.
- **An ok image over this release selects openkal-platform 0.20.0** or a later
  group naming openkal 0.19; `checks/hosted` and the image checks take theirs
  from 0.20.0.

## 0.2.4

- **openkal 0.18.** The implementations of openkal 0.18 (openkal-linux 0.19.0,
  openkal-macos 0.16.0, openkal-windows 0.14.0), which provide `openkal.map`;
  `checks/hosted` takes its C library and C++ run-time from openkal-platform
  0.19.0, whose okrun maps the image it runs. Emscripten takes
  openkal-emscripten 0.7.0, written against openkal 0.18.0, which leaves
  `openkal.map` absent.
- **An ok image over this release selects openkal-platform 0.19.0** or a
  later group naming openkal 0.18 (`platform = "openkal-platform@0.19.0"` under
  the `openkal` target, or `--platform`): the openkal rows of mcpp 2026.10.31.2
  pin 0.18.0, whose group names openkal 0.17.1.

## 0.2.3

- **openkal-linux 0.18.3.** Every directory handle is opened with `O_PATH`, so a
  program started by absolute name is found in a directory the process may
  search but not read. In the Android app sandbox (`run-as`, SELinux
  `runas_app`) the root directory could not be opened for reading, and every
  `process::spawn` by absolute path failed as not found (mcpp-community/mcpp#857).

## 0.2.2

- **Emscripten (`wasm32-emscripten`).** The package builds for a wasm module
  over Emscripten's C library and openkal-emscripten 0.6.0, under node or in a
  page. It adds `platform::emscripten`, `architecture::wasm32` and
  `is_emscripten`, and selects `src/targets/emscripten.cppm`,
  `src/arch/wasm32.cppm`, `src/emscripten/` and the C library's part of
  `src/posix/fs.cpp`. No platform module is defined. A program that starts
  another fails at link, naming the openkal symbol (`checks/absent`).
  - `interrupt` reports that no request to end is delivered. Its
    implementation over openkal's stop word moves to `src/kal/interrupt.cpp`,
    which every other target selects.
  - `machine` names `wasm32`, reports the processors Emscripten counts and
    does not know the memory.
  - A volume's kind is the file system Emscripten mounted there (`memfs`,
    `nodefs`, …). Standard output is diverted with the C library's
    descriptors, which openkal-emscripten's streams are.
  - CI builds the tests that do not start programs for the target and runs them
    under node (`checks/emscripten`, debug and release), along with the
    compile-time constants and the R5 checks.
- **`fs::read` reads into the string, not through a 64 KiB array on the
  stack.** That array was all of a wasm module's default stack (65536 bytes)
  and overflowed it.
- **An enquiry asks for the kind it reads absence from.** `stamp_of` and
  `identity_of` asked openkal for the size, the time or the identity, and then
  read the kind. An implementation fills only the fields it is asked for
  (openkal-emscripten does), so the kind stayed zero, which is
  `kal_node_absent`, and an existing file was reported not found.
- **The tests ask the volume before taking a lock or renaming without
  replacing,** and on a volume that does neither they check that the request is
  refused and changes nothing. A test that names its scratch directory after
  the program uses random bytes where no program is named.

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
