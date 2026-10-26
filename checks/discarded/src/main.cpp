// R5: the same calls in branches `if constexpr' discards are not odr-uses
// ([basic.def.odr]), so this program links on every system.
import std;
import mcpplibs.os;
import mcpplibs.os.linux;
import mcpplibs.os.macos;
import mcpplibs.os.windows;

namespace os = mcpplibs::os;

int main() {
    int n = 0;
    if constexpr (os::is_linux) n += static_cast<int>(os::linux::translator().has_value());
    if constexpr (os::is_macos) n += static_cast<int>(os::macos::translated());
    if constexpr (os::is_windows) n += static_cast<int>(os::windows::elevated());
    return n >= 0 ? 0 : 1;
}
