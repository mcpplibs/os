// mcpplibs.os.platform --- what the program is built for, and what it runs upon.
//
// The compile-time constants are the first-class interface (design part 2,
// R3): a program written for several systems dispatches with `if constexpr`,
// and a branch for another system is discarded rather than compiled. They come
// from one partition per target (src/targets/<os>.cppm), which the manifest
// selects, so no macro reaches this interface.
//
// `host()' names the system the program RUNS upon. Built for a system, that is
// the system it was built for, known when it is compiled, and `host()' is a
// constant expression. Built as an image (`<arch>-openkal'), the system is
// known only when the image is loaded; there `host()' is an ordinary function,
// and using it in a constant expression is a compile error that names it.
export module mcpplibs.os.platform;

import std;
export import :target;
export import :arch;

export namespace mcpplibs::os {

// The executable suffix and the separator of a list of directories, as the
// target's conventions spell them. A program built as an image asks `host()'
// and `conventions()' instead; these are not provided there.
inline constexpr bool is_linux   = PLATFORM == platform::linux;
inline constexpr bool is_macos   = PLATFORM == platform::macos;
inline constexpr bool is_windows = PLATFORM == platform::windows;
inline constexpr bool is_posix   = is_linux || is_macos;
// A build for the openkal target, `<arch>-openkal` (mcpp `cfg(os = "openkal")`): its artifact is an
// ok image (openkal SPEC clause 12), which a loader runs on every system. It does not say that the
// build is over openkal -- every build of this package is -- but which target it is built for, as
// each `is_<platform>` does (0.4.0; `is_image` before).
inline constexpr bool is_openkal = PLATFORM == platform::openkal;
[[deprecated("use is_openkal: a build for the openkal target, whose artifact is an ok image")]]
inline constexpr bool is_image   = is_openkal;
// A wasm module over Emscripten's C library (0.2.2): no program is started and
// no library loaded, the file system is the module's own (MEMFS), and the
// streams are the host's.
inline constexpr bool is_emscripten = PLATFORM == platform::emscripten;

// WHERE THIS BUILD CAN RUN, AND WHERE IT RUNS (0.4.0). `is_<platform>' says
// what the build is for: a native build of that system. A program that is
// also built as an image asks two questions instead -- can this build run on
// the platform (a constant: a native build runs on its own system, an image
// on every system a loader runs upon), and does it run there now (`host()',
// a constant on a native build and known when an image is loaded):
//
//   if constexpr (os::can_run_on(os::platform::windows))
//       if (os::host() == os::platform::windows)
//           on_drive = os::windows::on_dev_drive(dir);
//
// The first is what keeps another system's platform module out of a native
// build (a discarded statement is no odr-use, R5); the second is what an image
// decides by.
constexpr bool can_run_on(platform p) noexcept {
    if constexpr (is_openkal) return p == platform::linux || p == platform::macos || p == platform::windows;
    else return PLATFORM == p;
}

// The two questions for one call: `f' runs when this build runs on `P' now.
// `f' takes the platform as an argument (`[&](auto) { ... }'), which makes its
// body a template: a build that cannot run on `P' never instantiates it, so a
// platform module named inside is no odr-use there. A lambda without the
// parameter does not satisfy the constraint, because its body would be an
// odr-use of what it names on every build.
template <platform P, class F>
    requires std::invocable<F&, std::integral_constant<platform, P>>
void on(F&& f) {
    if constexpr (can_run_on(P))
        if (host() == P) std::invoke(f, std::integral_constant<platform, P>{});
}

// The name of a platform, as mcpp's target vocabulary and openkal's
// `kal_env_system' spell it.
constexpr std::string_view name(platform p) noexcept {
    switch (p) {
        case platform::linux:   return "linux";
        case platform::macos:   return "macos";
        case platform::windows: return "windows";
        case platform::openkal: return "openkal";
        case platform::emscripten: return "emscripten";
    }
    return "unknown";
}

constexpr std::string_view name(architecture a) noexcept {
    switch (a) {
        case architecture::x86_64:  return "x86_64";
        case architecture::aarch64: return "aarch64";
        case architecture::riscv64: return "riscv64";
        case architecture::wasm32:  return "wasm32";
    }
    return "unknown";
}

// The conventions a running program follows: the executable suffix, the list
// separator, whether names are compared without case by default. On a native
// target they are the target's; in an image they are the host's, chosen by
// `host()'.
struct conventions_t {
    std::string_view exe_suffix;
    char             path_list_separator;
    bool             case_insensitive_names;
};

constexpr conventions_t conventions_of(platform p) noexcept {
    return p == platform::windows ? conventions_t{ ".exe", ';', true }
                                  : conventions_t{ "", ':', p == platform::macos };
}

inline conventions_t conventions() { return conventions_of(host()); }

}  // namespace mcpplibs::os
