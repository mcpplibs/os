// Conventions of the host, and the terminal's colour policy.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

int main() {
    check(os::dirs::home().has_value(), "a home directory is named");
    check(os::dirs::config().has_value(), "a configuration directory is named");
    check(os::dirs::cache().has_value(), "a cache directory is named");
    check(!os::dirs::temp().empty(), "a temporary directory is named");
    if constexpr (os::is_linux) {
        check(os::dirs::cache()->generic_string().ends_with(".cache") || os::env::get("XDG_CACHE_HOME"),
              "Linux follows XDG");
    }
    // Not a terminal here (CI); colour follows NO_COLOR and CLICOLOR_FORCE.
    const bool forced = os::env::get("CLICOLOR_FORCE").value_or("0") != "0";
    check(os::terminal::color(os::terminal::stream::out) == (forced || os::terminal::interactive(os::terminal::stream::out)),
          "colour is written to a terminal or when forced");
    return g_failures;
}
