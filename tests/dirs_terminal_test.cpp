// Conventions of the host, and the terminal's colour policy.
import std;
import mcpplibs.os;
import mcpplibs.mtest;

namespace os = mcpplibs::os;
using namespace mcpplibs::mtest;

UTest dirs_terminal_tests{"dirs_terminal", [] {
    expect(os::dirs::home().has_value()) << "a home directory is named";
    expect(os::dirs::config().has_value()) << "a configuration directory is named";
    expect(os::dirs::cache().has_value()) << "a cache directory is named";
    expect(!os::dirs::temp().empty()) << "a temporary directory is named";
    if constexpr (os::is_linux) {
        expect(os::dirs::cache()->generic_string().ends_with(".cache") || os::env::get("XDG_CACHE_HOME")) << "Linux follows XDG";
    }
    // Not a terminal here (CI); colour follows NO_COLOR and CLICOLOR_FORCE.
    const bool forced = os::env::get("CLICOLOR_FORCE").value_or("0") != "0";
    expect(os::terminal::color(os::terminal::stream::out) == (forced || os::terminal::interactive(os::terminal::stream::out))) << "colour is written to a terminal or when forced";
}};
