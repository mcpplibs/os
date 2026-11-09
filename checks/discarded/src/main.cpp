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
    // A member template too (0.4.4): its definition is not instantiated where it is not used.
    if constexpr (os::is_windows) n += static_cast<int>(os::windows::com::object{}.call(3, 0).has_value());
    if constexpr (os::is_macos) n += static_cast<int>(os::macos::objc::send<int>(nullptr, "hash"));
    // The same calls in the form that also holds in an image (0.4.0).
    if constexpr (os::can_run_on(os::platform::linux))
        if (os::host() == os::platform::linux) n += static_cast<int>(os::linux::translator().has_value());
    os::on<os::platform::macos>([&](auto) { n += static_cast<int>(os::macos::translated()); });
    os::on<os::platform::windows>([&](auto) { n += static_cast<int>(os::windows::elevated()); });
    return n >= 0 ? 0 : 1;
}
