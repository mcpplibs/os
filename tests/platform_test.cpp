// The platform a program is built for and the host it runs upon (R3; part 2
// E29). Built for a system the host is a constant expression; built as an ok
// image it is known when the image is loaded. The native rows also build
// checks/constants, where the compile-time facts are static assertions.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

static_assert(os::is_linux + os::is_macos + os::is_windows + os::is_image == 1);
static_assert(os::is_posix == (os::is_linux || os::is_macos));
static_assert(os::name(os::ARCH) == "x86_64" || os::name(os::ARCH) == "aarch64" || os::name(os::ARCH) == "riscv64");

int main() {
    const auto host = os::host();
    check(host != os::platform::openkal, "the host is a system, whatever the target");
    if constexpr (!os::is_image) check(host == os::PLATFORM, "built for a system, the host is that system");
    else check(os::env::loader().has_value(), "an image is told the program that runs it");
    check(os::conventions().exe_suffix == (host == os::platform::windows ? ".exe" : ""),
          "the host's conventions are the host's");
    const auto system = os::env::system();
    check(system && *system == os::name(host), "openkal names the system the program runs upon");
    check(!os::machine::arch().empty(), "the machine's architecture is named");
    if constexpr (!os::is_image)
        check(os::machine::memory().value_or(0) > 0, "the machine's memory is known");
    return g_failures;
}
