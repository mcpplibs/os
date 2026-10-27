# mcpplibs.os

A C++ operating-system library over [openkal](https://github.com/mcpplibs/openkal):
processes, files, the environment, terminals, conventional directories and local IPC,
with compile-time platform constants and a few platform modules.

```toml
[dependencies]
os = { namespace = "mcpplibs", version = "0.2.1" }
```

```cpp
import std;
import mcpplibs.os;
namespace os = mcpplibs::os;

int main() {
    auto r = os::process::run({ .program = "git", .args = { "--version" } });
    if (r) std::print("{}", r->out);
    if constexpr (os::is_windows) { /* compiled only for Windows */ }
}
```

## Layers

| Layer | What it is | Where it is |
|---|---|---|
| openkal | the kernel ABI: minimal composable atoms, one implementation per system | `mcpplibs/openkal`, `openkal-<os>` |
| **mcpplibs.os** | composition and conventions: PATH search, atomic and non-replacing publication, process trees, directory conventions, colour policy, local IPC | this package |
| a program | its domain | — |

The kernel layer is openkal and only openkal. Native code in this package serves only
what is not a kernel question: the machine beneath the system (`machine`), the dynamic
loader (`library`: the C library's on Linux and macOS, kernel32's on Windows), and the
platform modules.

## The program's C library

A program over this package chooses its C library. It can be the system's own (glibc,
libSystem, the Microsoft C runtime), or one that sits on openkal on the system's own
triple: openkal-musl with openkal-llvm-runtime (mcpp `docs/24`). The package behaves the
same under either.

Native code beside openkal reaches each system through that system's stable interface,
never through the C library, because a C library on openkal presents POSIX and openkal
rather than the system:

| System | How native code reaches it |
|---|---|
| Linux | the kernel's calls, one trap per architecture (`src/linux/kernel.h`) |
| macOS | the kernel's calls (`src/macos/kernel.h`), and the libSystem names no C library defines (`src/macos/darwin.h`) |
| Windows | kernel32 and advapi32, with DWORD as 32 bits and WCHAR as UTF-16 under every C environment (`src/windows/win32.h`) |

The standard output's diversion and the ending for a closed reader act on the process
itself: the kernel's descriptors and dispositions, or on Windows the process's standard
handles. `checks/hosted` runs the tests under openkal-musl and openkal-llvm-runtime on
every system in CI.

## Modules

| Module | Contents |
|---|---|
| `mcpplibs.os.platform` | `PLATFORM`, `ARCH`, `is_linux` / `is_macos` / `is_windows` / `is_posix`, `exe_suffix`, `path_list_separator`; `host()`, a constant expression on a native target |
| `mcpplibs.os.error` | `errc` (openkal's closed set of conditions), `error`, `result<T>` |
| `mcpplibs.os.env` | variables (names without case on Windows), arguments, the program's own image, lists of directories in the host's separator (`split_list`, `join_list`), PATH search with PATHEXT |
| `mcpplibs.os.fs` | `write_atomic`, `publish` (no replace), `append`, `sync`, `caps`, `lock`, links, `set_executable`, `stamp_of`, `identity_of`, `owner_of`, `writable`, `locate`, `absolute`, `names_a_place`; what a volume does, measured (`measure`, `volume_type`), and the operations that follow it (`place`, `exchange`); names in the host's rules without the file system: `generic`, `native`, `extended`, `is_absolute`, `same`, `within` |
| `mcpplibs.os.process` | `spawn`, `run`, `exec`, a command line joined as it is for the command interpreter (`one_string`), pipes, both outputs on one pipe, output onto this program's error output, units and `end_unit`, bound lifetime, bounded waits and reads; `self`, `describe`, `alive` |
| `mcpplibs.os.interrupt` | `requested`, `wait`, `end_as_requested`, `end_as_output_closed` |
| `mcpplibs.os.stdio` | the three streams as bytes; a bounded read of input; the output sent to the error stream for a while (`output_to_error`) |
| `mcpplibs.os.terminal` | `interactive`, `size`, `sequences`, `color`, `raw_mode` (with or without the interrupting keystroke), `foreground` |
| `mcpplibs.os.dirs` | `home`, `config`, `data`, `state`, `cache`, `runtime`, `temp` |
| `mcpplibs.os.ipc` | a listener and a connection at a name in a directory; `single_instance` |
| `mcpplibs.os.net` | loopback TCP |
| `mcpplibs.os.machine` | the machine's architecture (through Rosetta and WoW64), memory and the memory available now, processors (logical, physical, of differing speed) |
| `mcpplibs.os.random` | bytes from the system's source of unpredictable bytes; `hex` |
| `mcpplibs.os.library` | shared libraries at run time: `open` (a path or a name the system searches for), `loaded` (one the process has, without loading it), a handle's `symbol` / `function<F>`, `process_symbol` (as the loader resolves one), `suffix` |
| `mcpplibs.os.linux` / `.macos` / `.windows` / `.posix` | platform modules: declared on every target, defined only on their own |

## Rules

- **R0.** What std does, this package does not repeat.
- **R1.** A portable facility exists once, in `os::<area>`.
- **R2.** A platform module holds only what has no portable meaning. Each of its declarations
  says why.
- **R3.** Compile-time platform constants are first-class. `host()` names the running system:
  - on a native target it equals `PLATFORM` and is a constant expression;
  - in an image (`<arch>-openkal`) it is an ordinary function, and using it in a constant
    expression is a compile error.
- **R4.** A native handle does not appear in a portable type.
- **R5.** A platform module is declared on every target and defined only on its own. A call
  outside `if constexpr` for that platform is a link error. The same call in a discarded branch
  is not an odr-use, so it links. CI asserts both.
- **R6.** Where openkal is not in its shared form, this package's resources do not cross a
  shared-object boundary.
- **R7.** A name reaches the system as UTF-8 and comes back as UTF-8 on every host, never
  through the process's code page. `path::string()` and `path(std::string)` use the ANSI code
  page on Windows, where a name the code page cannot spell throws and one it can is not UTF-8.
  `os::fs::utf8()`, `generic_utf8()` and `from_utf8()` are the conversions, and the package uses
  nothing else. The tests write, read and rename names in four scripts.

Interface units import `std` and contain no system header and no macro. Implementation units
on openkal import only `openkal.*`. The Windows and macOS units declare the functions they
call themselves (`src/windows/win32.h`, `src/macos/darwin.h`). Linux's and macOS's kernel
calls are in `src/linux/arch/` and `src/macos/arch/`, which the manifest selects by
architecture.

## As an ok image

Built for `<arch>-openkal` (openkal SPEC clause 12), a program over this library
is one image per architecture that `okrun` runs on Linux, macOS and Windows.
`os::PLATFORM` is `platform::openkal` and `os::is_image` is true; `os::host()`
is an ordinary function there, answered when the image is loaded, and
`os::conventions()` follows it; `exe_suffix` and `path_list_separator` are not
provided, so code that reads them is a compile error rather than a wrong answer
on another system. `os::env::loader()` names the program that runs the image,
and `os::env::self_command()` is what starts the same program again --- a
loader and an image, or the image alone when it is built for its system. A
name's spelling is the host's as well: `os::fs::absolute()` and
`os::fs::names_a_place()` read `C:\x` and `bin\x` as Windows does when the
image runs there, where the standard library inside an image reads names as
POSIX does. The
image carries its C library and C++ run-time (`openkal-llvm-runtime`); the
tests are built as images in CI and run through okrun on four systems, and
`checks/constants` holds the compile-time facts of a native target.

## License

Apache-2.0
