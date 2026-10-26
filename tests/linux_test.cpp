// mcpplibs.os.linux: the system-call translator a program runs under, from
// what it reads (the facts), and for this program.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

int main() {
    if constexpr (os::is_linux) {
        using facts = os::linux::translator_facts;
        auto is = [](const facts& f) { return os::linux::classify_translator(f).value_or(""); };
        check(is(facts{ "TracerPid:\t812\n", "proot\n", {} }) == "proot", "a program traced by proot is under PRoot");
        check(is(facts{ "TracerPid:\t812\n", "proot-static", {} }) == "proot", "whatever proot* is called");
        check(is(facts{ "TracerPid:\t812\n", "gdb\n", {} }).empty(), "a debugger is not a translator");
        check(is(facts{ "TracerPid:\t812\n", "", { "PATH=/bin", "PROOT_TMP_DIR=/tmp" } }) == "proot",
              "the PROOT_ variables decide when the tracer cannot be read");
        check(is(facts{ "", "", { "PROOT_TMP_DIR=/tmp" } }) == "proot", "and when the status cannot be read");
        check(is(facts{ "", "", { "PATH=/bin", "NOT_PROOT_X=1" } }).empty(), "and name nothing without them");
        check(is(facts{ "Name:\tx\nTracerPid:\t0\n", "", { "PROOT_NO_SECCOMP=1" } }).empty(),
              "a PROOT_ variable in a program nothing traces means nothing");
        check(is(facts{ "TracerPid:\tnot-a-number\n", "proot", {} }).empty(), "a status that names no tracer names none");
        const auto here = os::linux::translator();
        check(!here || *here == "proot", "this program's translator is PRoot or none");
        if (const auto expected = os::env::get("OS_EXPECT_TRANSLATOR"))
            check(here.value_or("") == *expected, "this program is under the translator its environment says");
    }
    return g_failures;
}
