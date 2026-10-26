// The compile-time constants, and the host they agree with (R3; part 2 E29).
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

// A constant expression on a native target: these are compiled, not run.
static_assert(os::host() == os::PLATFORM);
static_assert(os::is_linux + os::is_macos + os::is_windows == 1);
static_assert(os::is_posix == (os::is_linux || os::is_macos));
static_assert(!os::is_image);
static_assert(os::name(os::ARCH) == "x86_64" || os::name(os::ARCH) == "aarch64" || os::name(os::ARCH) == "riscv64");
static_assert(os::exe_suffix == (os::is_windows ? ".exe" : ""));

int main() {
    if constexpr (os::host() == os::platform::windows) {
        check(os::path_list_separator == ';', "a Windows target separates PATH with ';'");
    } else {
        check(os::path_list_separator == ':', "a POSIX target separates PATH with ':'");
    }
    check(os::conventions().exe_suffix == os::exe_suffix, "the host's conventions are the target's");
    const auto system = os::env::system();
    check(system && *system == os::name(os::host()), "openkal names the system the program runs upon");
    check(!os::machine::arch().empty(), "the machine's architecture is named");
    check(os::machine::memory().value_or(0) > 0, "the machine's memory is known");
    return g_failures;
}
