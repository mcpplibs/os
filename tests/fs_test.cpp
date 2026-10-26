// mcpplibs.os.fs upon a scratch directory.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;
namespace stdfs = std::filesystem;

int main() {
    const auto dir = stdfs::temp_directory_path() / std::format("mcpplibs-os-fs-{}", os::process::self()->id);
    stdfs::remove_all(dir);
    stdfs::create_directories(dir);

    const auto l = os::fs::locate(dir);
    check(l.has_value(), "the scratch directory lies beneath a supplied directory");

    const auto file = dir / "a.txt";
    check(os::fs::write(file, "one").has_value(), "a file is written");
    check(os::fs::read(file).value_or("") == "one", "and read back");
    check(os::fs::write_atomic(file, "two").has_value(), "and replaced all at once");
    check(os::fs::read(file).value_or("") == "two", "and the replacement is read");

    const auto caps = os::fs::caps(dir);
    check(caps.has_value(), "the volume says what it can do");
    const auto pub = dir / "published.txt";
    auto first = os::fs::publish(pub, "first");
    check(first.has_value(), "a name nobody has taken is published");
    if (caps && caps->rename_no_replace) check(first && first->atomic, "and atomically where the volume can");
    auto second = os::fs::publish(pub, "second");
    check(!second && second.error().code == os::errc::exists, "a published name is not replaced");
    check(os::fs::read(pub).value_or("") == "first", "and keeps what was published first");

    {
        auto a = os::fs::lock::acquire(dir / "lock", os::fs::lock::kind::exclusive, false);
        check(a.has_value() && a->held(), "an exclusive lock is taken");
    }
    auto again = os::fs::lock::acquire(dir / "lock", os::fs::lock::kind::exclusive, false);
    check(again.has_value(), "and is taken again once released");
    // A lock held for the rest of the program: the directory is removed below
    // while it is held, where the volume removes a held name at once.

    const auto s = os::fs::stamp_of(file);
    check(s && s->size == 3, "a stamp reports the size");
    const auto i1 = os::fs::identity_of(file);
    const auto i2 = os::fs::identity_of(dir / "." / "a.txt");
    check(i1 && i2 && *i1 == *i2, "two names of one node have one identity");

    if (caps && caps->make_links) {
        stdfs::create_directories(dir / "target");
        const auto made = os::fs::create_link("target", dir / "link", true);
        check(made.has_value() || made.error().code == os::errc::permission, "a directory link is made where the volume can");
        if (made) {
            const auto t = os::fs::read_link(dir / "link");
            check(t && t->find("target") != std::string::npos, "and reads back its target");
            check(stdfs::exists(dir / "link" / "."), "and reaches the directory");
        }
    }
    check(os::fs::rename(file, dir / "b.txt", false).has_value(), "a rename that does not replace renames to a free name");

    check(os::fs::names_a_place("bin/tool") && !os::fs::names_a_place("tool"),
          "a name with a separator says where a file is, a bare one does not");
    check(os::fs::names_a_place("C:tool") == (os::host() == os::platform::windows)
              && os::fs::names_a_place("bin\\tool") == (os::host() == os::platform::windows),
          "a drive and a backslash say where a file is on Windows and nowhere else");

    if (caps && caps->remove_open) {
        std::error_code ec;
        stdfs::remove_all(dir, ec);
        check(!ec && !stdfs::exists(dir), "a directory holding a file held open is removed");
    } else {
        if (again) again->release();
        stdfs::remove_all(dir);
    }
    return g_failures;
}
