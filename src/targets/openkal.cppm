// The target of this build: an image (`<arch>-openkal'), which runs upon any
// system its architecture has a loader for. Selected by [target.openkal.build].
export module mcpplibs.os.platform:target;

import std;

export namespace mcpplibs::os {

enum class platform { linux, macos, windows, openkal, emscripten };

inline constexpr platform PLATFORM = platform::openkal;

// Known when the image is loaded and not before. An ordinary declaration, so
// that a constant expression using it is an error naming it (part 2, E29);
// `if consteval' would say the same and is refused by one compiler (E28).
platform host() noexcept;

}  // namespace mcpplibs::os
