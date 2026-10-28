// The target of this build: emscripten, a C library over a JavaScript host
// (node, or a page). Selected by the manifest's [target.'cfg(os = "emscripten")'.build].
export module mcpplibs.os.platform:target;

import std;

export namespace mcpplibs::os {

enum class platform { linux, macos, windows, openkal, emscripten };

// What the program is built for.
inline constexpr platform PLATFORM = platform::emscripten;

// What the program runs upon: the same module runs under node and in a page,
// and the conventions it follows are its C library's and MEMFS's in both.
constexpr platform host() noexcept { return PLATFORM; }

inline constexpr std::string_view exe_suffix = "";
inline constexpr char path_list_separator = ':';

}  // namespace mcpplibs::os
