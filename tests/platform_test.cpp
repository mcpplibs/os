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
    // Linux translates no program to another processor's instructions: the
    // machine is named as the processor this program runs on, whichever C
    // library answers the program's other questions.
    if constexpr (os::is_linux)
        check(os::machine::arch() == os::name(os::ARCH), "Linux: the machine is the processor the program runs on");
    if constexpr (!os::is_image)
        check(os::machine::memory().value_or(0) > 0, "the machine's memory is known");
    const auto a = os::random::hex(16), b = os::random::hex(16);
    check(a && a->size() == 32 && b && *a != *b, "the system's source gives bytes that differ from call to call");
        {
        const auto p = os::machine::processors();
        check(p.logical >= 1 && p.physical >= 1 && p.physical <= p.logical, "processors: logical and physical counted");
        if (!os::is_image) check(os::machine::memory_available().value_or(0) > 0, "and the memory available now");
    }
    if constexpr (os::is_windows) {
        check(os::windows::code_page() != 0, "Windows: the ANSI code page is known");
        (void)os::windows::long_paths_enabled();
    }
    return g_failures;
}
