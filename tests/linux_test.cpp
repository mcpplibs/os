// mcpplibs.os.linux: the system-call translator a program runs under, from
// what it reads (the facts), and for this program.
import std;
import mcpplibs.os;
import mcpplibs.mtest;

namespace os = mcpplibs::os;
using namespace mcpplibs::mtest;

UTest linux_tests{"linux", [] {
    if constexpr (os::is_linux) {
        using facts = os::linux::translator_facts;
        auto is = [](const facts& f) { return os::linux::classify_translator(f).value_or(""); };
        expect(is(facts{ "TracerPid:\t812\n", "proot\n", {} }) == "proot") << "a program traced by proot is under PRoot";
        expect(is(facts{ "TracerPid:\t812\n", "proot-static", {} }) == "proot") << "whatever proot* is called";
        expect(is(facts{ "TracerPid:\t812\n", "gdb\n", {} }).empty()) << "a debugger is not a translator";
        expect(is(facts{ "TracerPid:\t812\n", "", { "PATH=/bin", "PROOT_TMP_DIR=/tmp" } }) == "proot") << "the PROOT_ variables decide when the tracer cannot be read";
        expect(is(facts{ "", "", { "PROOT_TMP_DIR=/tmp" } }) == "proot") << "and when the status cannot be read";
        expect(is(facts{ "", "", { "PATH=/bin", "NOT_PROOT_X=1" } }).empty()) << "and name nothing without them";
        expect(is(facts{ "Name:\tx\nTracerPid:\t0\n", "", { "PROOT_NO_SECCOMP=1" } }).empty()) << "a PROOT_ variable in a program nothing traces means nothing";
        expect(is(facts{ "TracerPid:\tnot-a-number\n", "proot", {} }).empty()) << "a status that names no tracer names none";
        const auto here = os::linux::translator();
        expect(!here || *here == "proot") << "this program's translator is PRoot or none";
        if (const auto expected = os::env::get("OS_EXPECT_TRANSLATOR"))
            expect(here.value_or("") == *expected) << "this program is under the translator its environment says";
    }
}};
