# mcpplibs.os

A C++ operating-system library over [openkal](https://github.com/mcpplibs/openkal):
processes, files, the environment, terminals, conventional directories and local IPC,
with compile-time platform constants and a few platform modules.

```toml
[dependencies]
os = { namespace = "mcpplibs", version = "0.1.0" }
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
what is not a kernel question: the machine beneath the system (`machine`), and the platform
modules.

## Modules

| Module | Contents |
|---|---|
| `mcpplibs.os.platform` | `PLATFORM`, `ARCH`, `is_linux` / `is_macos` / `is_windows` / `is_posix`, `exe_suffix`, `path_list_separator`; `host()`, a constant expression on a native target |
| `mcpplibs.os.error` | `errc` (openkal's closed set of conditions), `error`, `result<T>` |
| `mcpplibs.os.env` | variables, arguments, the program's own image, PATH search with PATHEXT |
| `mcpplibs.os.fs` | `write_atomic`, `publish` (no replace), `caps`, `lock`, links, `set_executable`, `stamp_of`, `identity_of`, `locate` |
| `mcpplibs.os.process` | `spawn`, `run`, pipes, units, bound lifetime, bounded waits; `self`, `describe`, `alive` |
| `mcpplibs.os.interrupt` | `requested`, `wait`, `end_as_requested` |
| `mcpplibs.os.stdio` | the three streams as bytes |
| `mcpplibs.os.terminal` | `interactive`, `size`, `sequences`, `color`, `raw_mode` |
| `mcpplibs.os.dirs` | `home`, `config`, `data`, `state`, `cache`, `runtime`, `temp` |
| `mcpplibs.os.ipc` | a listener and a connection at a name in a directory; `single_instance` |
| `mcpplibs.os.net` | loopback TCP |
| `mcpplibs.os.machine` | the machine's architecture (through Rosetta and WoW64), memory |
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

Interface units import `std` and contain no system header and no macro. Implementation units
on openkal import only `openkal.*`. The Windows units declare the functions they call
themselves (`src/windows/win32.h`).

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
`os::fs::names_a_place()` read `C:\\x` and `bin\\x` as Windows does when the
image runs there, where the standard library inside an image reads names as
POSIX does. The
image carries its C library and C++ run-time (`openkal-llvm-runtime`); the
tests are built as images in CI and run through okrun on four systems, and
`checks/constants` holds the compile-time facts of a native target.

## License

Apache-2.0
