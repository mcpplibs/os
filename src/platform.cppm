// mcpplibs.os.platform --- what the program is built for, and what it runs upon.
//
// The compile-time constants are the first-class interface (design part 2,
// R3): a program written for several systems dispatches with `if constexpr`,
// and a branch for another system is discarded rather than compiled. They come
// from one partition per target (src/target/<os>.cppm), which the manifest
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

export namespace mcpplibs::os {

// The executable suffix and the separator of a list of directories, as the
// target's conventions spell them. A program built as an image asks `host()'
// and `conventions()' instead; these are not provided there.
inline constexpr bool is_linux   = PLATFORM == platform::linux;
inline constexpr bool is_macos   = PLATFORM == platform::macos;
inline constexpr bool is_windows = PLATFORM == platform::windows;
inline constexpr bool is_posix   = is_linux || is_macos;
inline constexpr bool is_image   = PLATFORM == platform::openkal;

// The name of a platform, as mcpp's target vocabulary and openkal's
// `kal_env_system' spell it.
constexpr std::string_view name(platform p) noexcept {
    switch (p) {
        case platform::linux:   return "linux";
        case platform::macos:   return "macos";
        case platform::windows: return "windows";
        case platform::openkal: return "openkal";
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
