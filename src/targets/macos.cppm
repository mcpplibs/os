// The target of this build: macos. Selected by the manifest's [target.macos.build].
export module mcpplibs.os.platform:target;

import std;

export namespace mcpplibs::os {

enum class platform { linux, macos, windows, openkal };

// What the program is built for.
inline constexpr platform PLATFORM = platform::macos;

// What the program runs upon: on a native target, what it was built for.
constexpr platform host() noexcept { return PLATFORM; }

inline constexpr std::string_view exe_suffix = PLATFORM == platform::windows ? ".exe" : "";
inline constexpr char path_list_separator = PLATFORM == platform::windows ? ';' : ':';

}  // namespace mcpplibs::os
