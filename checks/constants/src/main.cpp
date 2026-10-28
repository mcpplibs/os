// Built on every native row and for Emscripten: each fact is a constant
// expression there.
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

static_assert(os::host() == os::PLATFORM);
static_assert(os::is_linux + os::is_macos + os::is_windows + os::is_emscripten == 1);
static_assert(!os::is_image);
static_assert(os::exe_suffix == (os::is_windows ? ".exe" : ""));
static_assert(os::path_list_separator == (os::is_windows ? ';' : ':'));
static_assert(os::conventions_of(os::host()).exe_suffix == os::exe_suffix);

int main() {
    if constexpr (os::host() == os::platform::windows) return os::path_list_separator == ';' ? 0 : 1;
    else return os::path_list_separator == ':' ? 0 : 1;
}
