// Shared libraries at run time: a system library opened by name and its
// functions called; a library the process already has, found without loading
// it; a symbol of the program; and a library that is not there, an error.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

int main() {
    if (!os::library::supported()) {
        // An ok image, or a program linked statically (aarch64 Linux's GCC links musl statically).
        auto none = os::library::open("anything");
        check(!none && none.error().code == os::errc::not_supported, "a static program loads no library, and says so");
        return g_failures;
    }
    // A library every process of the system has, and a function of it.
    const std::string_view system = os::is_windows ? "kernel32.dll" : os::is_macos ? "/usr/lib/libSystem.B.dylib" : "libc.so.6";
    const std::string_view function = os::is_windows ? "GetCurrentProcessId" : "getpid";
    auto lib = os::library::open(std::filesystem::path(std::string(system)));
    check(lib.has_value(), "a system library opens by name");
    if (lib) {
        check(lib->symbol(function) != nullptr, "and defines its functions");
        check(lib->symbol("no_such_function_anywhere") == nullptr, "and not others");
        // (x86_64 and aarch64 Windows have one calling convention.)
        if constexpr (os::is_windows) check(lib->function<unsigned long()>(function)() != 0, "its function is called");
        else check(lib->function<int()>(function)() > 0, "its function is called");
    }
    auto present = os::library::loaded(system);
    check(present.has_value() && present->symbol(function) != nullptr, "a library in the process is found without loading it");
    check(!os::library::loaded("libnothing-loaded-here.so.0").has_value(), "and one that is not, is not");
    check(os::library::process_symbol(function) != nullptr, "the program's symbols resolve as the loader resolves them");
    auto missing = os::library::open("libmcpplibs-os-no-such-library.so");
    check(!missing && missing.error().code == os::errc::not_found && !missing.error().message().empty(), "a missing library is an error");
    check(os::library::suffix() == (os::is_windows ? ".dll" : os::is_macos ? ".dylib" : ".so"), "the system's suffix");
    // Moving a handle keeps the library open once.
    if (lib) {
        os::library::handle moved = std::move(*lib);
        check(moved.valid() && !lib->valid(), "a handle moves");
    }
    return g_failures;
}
