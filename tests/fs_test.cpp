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

    {
        // Names in the host's rules, without the file system.
        const bool windows = os::host() == os::platform::windows;
        check(os::fs::generic("a/./b/../c/") == "a/c", "a name is spelt with its dots resolved and no trailing separator");
        check(os::fs::generic("/x/../../y") == "/y", "and nothing is above the root");
        check(os::fs::is_absolute("/usr/bin") == !windows, "a leading separator is a root on POSIX");
        if (windows) {
            check(os::fs::generic("C:\\Users\\x\\") == "C:/Users/x", "a Windows name is spelt with forward separators");
            check(os::fs::native("C:/Users/x") == "C:\\Users\\x", "and the host's separator for a person or a program");
            check(os::fs::is_absolute("C:/x") && !os::fs::is_absolute("C:x") && os::fs::is_absolute("//srv/share/x"),
                  "a drive and a separator, or a share, is a root; a drive alone is not");
            check(os::fs::same("C:/Users/X", "c:\\users\\x"), "names are the same without regard to case");
        } else {
            check(os::fs::native("a/b") == "a/b", "a POSIX name is its own native spelling");
            check(os::fs::same("/a/b", "/a/./b/") && (os::fs::same("/a/B", "/a/b") == (os::host() == os::platform::macos)),
                  "names compare by components, and by case where the host's names do");
        }
        check(os::fs::within("/a/b/c", "/a/b") && os::fs::within("/a/b", "/a/b") && !os::fs::within("/a/bc", "/a/b"),
              "a name is within a directory by components");
        const char sep = os::conventions().path_list_separator;
        const std::string list = std::string("/x") + sep + sep + " /y " + sep + (windows ? "\"C:/with space\"" : "/z");
        const auto parts = os::env::split_list(list);
        check(parts.size() == 3 && parts[0] == "/x" && parts[1] == "/y", "a list splits at the host's separator and skips empty entries");
        if (windows) check(parts.size() == 3 && parts[2] == "C:/with space", "and an entry quoted whole is unquoted");
        const std::vector<std::filesystem::path> dirs{ "/p", "/q" };
        check(os::env::split_list(os::env::join_list(dirs)).size() == 2, "and joining is its inverse");
    }

    {
        // Names the Windows code page cannot spell, both ways through the
        // system: UTF-8 in and out on every host, never the ANSI code page.
        const std::string name = "\xE4\xB8\xAD\xE6\x96\x87 \xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93 \xD0\xBA\xD0\xB8\xD1\x80 \xE0\xA4\x95";
        check(os::fs::utf8(os::fs::from_utf8(name)) == name, "a UTF-8 name survives the trip through a path");
        const auto sub = dir / os::fs::from_utf8(name);
        stdfs::create_directories(sub);
        const auto f = sub / os::fs::from_utf8(name + ".txt");
        check(os::fs::write(f, "\xE4\xBD\xA0\xE5\xA5\xBD").has_value(), "a file whose name is not ASCII is written");
        check(os::fs::read(f).value_or("") == "\xE4\xBD\xA0\xE5\xA5\xBD", "and read back");
        check(stdfs::exists(f), "and the system sees it under that name");
        check(os::fs::write_atomic(f, "again").has_value() && os::fs::read(f).value_or("") == "again", "replaced all at once");
        const auto moved = sub / os::fs::from_utf8("\xE6\x96\xB0 " + name);
        check(os::fs::rename(f, moved, false).has_value() && stdfs::exists(moved), "and renamed");
        const auto missing = os::fs::read(sub / os::fs::from_utf8("\xE6\xB2\xA1\xE6\x9C\x89"));
        check(!missing && missing.error().message().contains("\xE6\xB2\xA1\xE6\x9C\x89"), "an error names the file in UTF-8");
        check(os::fs::generic_utf8(os::fs::absolute(sub)).ends_with(name), "absolute keeps the name");
    }

    if (caps && caps->remove_open) {
        std::error_code ec;
        stdfs::remove_all(dir, ec);
        check(!ec && !stdfs::exists(dir), "a directory holding a file held open is removed");
    } else {
        if (again) again->release();
        stdfs::remove_all(dir);
    }
        {
        // 0.2.0: appending, durability, measured volumes, placing, exchanging.
        const auto dir = std::filesystem::temp_directory_path()
            / std::format("mcpplibs-os-fs2-{}", os::process::self()->id);
        std::filesystem::create_directories(dir);
        check(os::fs::append(dir / "log", "a").has_value() && os::fs::append(dir / "log", "b").has_value()
                  && os::fs::read(dir / "log").value_or("") == "ab",
              "appends land whole at the end, the file created by the first");
        check(os::fs::sync(dir / "log").has_value() && os::fs::sync(dir).has_value(),
              "a file and a directory are made durable");
        const auto b = os::fs::measure(dir);
        check(b.measured, "a writable directory's volume is measured");
        // The kind of a volume is the system's to name; an image is not told it.
        if constexpr (!os::is_image)
            check(!b.type.empty() && b.type != "0x0", "and its kind is named by the system");
        (void)os::fs::write(dir / "x", "x");
        check(os::fs::place(dir / "x", dir / "y").has_value() && os::fs::read(dir / "y").value_or("") == "x"
                  && !std::filesystem::exists(dir / "x"),
              "an entry is placed under a free name");
        (void)os::fs::write(dir / "z", "z");
        const auto refused = os::fs::place(dir / "z", dir / "y");
        check(!refused && os::fs::read(dir / "y").value_or("") == "x", "and never over one that is there");
        (void)os::fs::write(dir / "p", "p");
        (void)os::fs::write(dir / "q", "q");
        check(os::fs::exchange(dir / "p", dir / "q").has_value()
                  && os::fs::read(dir / "p").value_or("") == "q" && os::fs::read(dir / "q").value_or("") == "p",
              "two entries trade places");
        if (!os::is_image) {
            check(os::fs::owner_of(dir) == os::fs::owner::this_user, "a directory this program made is its user's");
            check(os::fs::writable(dir), "and this user may write it");
        }
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    check(os::fs::windows_extended("C:/a/./b/../c") == "\\\\?\\C:\\a\\c", "a drive name in the long spelling, resolved");
    check(os::fs::windows_extended("//server/share/x") == "\\\\?\\UNC\\server\\share\\x", "a share in the long spelling");
    check(os::fs::windows_extended("C:/..") == "\\\\?\\C:\\", "and `..' never climbs above the drive");
    check(os::fs::windows_extended("rel/x") == "rel/x", "a relative name is not this rule's");
    if (os::host() != os::platform::windows) check(os::fs::extended("/a/b") == "/a/b", "elsewhere the name is itself");
    return g_failures;
}
