// The platform modules where this build runs (0.4.0): the same questions in a
// native build and an image, asked through `on<P>', so that this one source
// links and answers on every row. The values are the CI runners': Windows
// runs elevated, Linux runs under no translator, macOS runs images natively.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

static_assert(os::can_run_on(os::PLATFORM) || os::is_image, "a native build runs on its own system");
static_assert(!os::can_run_on(os::platform::openkal), "an image is a target, never a system it runs upon");
static_assert(!os::can_run_on(os::platform::emscripten) || os::is_emscripten, "no image runs under Emscripten");
static_assert(!os::is_image || (os::can_run_on(os::platform::linux) && os::can_run_on(os::platform::macos)
                                && os::can_run_on(os::platform::windows)),
              "an image can run on every system a loader runs upon");

// The effective user as the kernel names it, and as `id -u' does. A template,
// so that it is instantiated, and posix's name linked, only where `on<P>'
// calls it.
template <os::platform P>
void check_effective_user(std::integral_constant<os::platform, P>, const char* system) {
    const auto uid = os::posix::effective_user();
    auto id = os::process::run({.program = "/usr/bin/id", .args = {"-u"}, .err = os::process::stdio::null});
    const std::string named = id ? std::string(id->out.substr(0, id->out.find_first_of("\r\n"))) : "(id did not run)";
    std::printf("  note  effective_user() = %u, id -u = %s\n", uid, named.c_str());
    check(std::to_string(uid) == named, std::format("{}: the effective user is the one `id -u' names", system).c_str());
}

int main() {
    const bool ci = os::env::get("GITHUB_ACTIONS").has_value();
    int asked = 0;
    os::on<os::platform::linux>([&](auto p) {
        ++asked;
        check(!os::linux::translator().has_value() || !ci, "Linux: the runner is no translator");
        check_effective_user(p, "Linux");
    });
    os::on<os::platform::macos>([&](auto p) {
        ++asked;
        check(!os::macos::translated() || !os::is_image, "macOS: an image is never translated");
        check_effective_user(p, "macOS");
    });
    os::on<os::platform::windows>([&](auto) {
        ++asked;
        check(os::windows::elevated() || !ci, "Windows: the runner's token is the administrator's");
        const auto dev = os::windows::on_dev_drive(std::filesystem::current_path());
        std::printf("  note  on_dev_drive(cwd) = %s\n", dev ? (*dev ? "true" : "false") : "not known");
        check(!dev.value_or(false) || !ci, "Windows: the runner's work directory is on no developer volume");
    });
    check(asked == (os::is_emscripten ? 0 : 1), "exactly one platform answers where the build runs");
    return g_failures;
}
