// mcpplibs.os.fs::watcher upon a scratch directory: what changed, each once,
// and nothing when nothing did; a save by rename; names made and removed; a
// directory's entries; a wait that ends when a change comes.
import std;
import mcpplibs.os;
import mcpplibs.mtest;

namespace os = mcpplibs::os;
namespace stdfs = std::filesystem;
using namespace mcpplibs::mtest;
using namespace std::chrono_literals;

namespace {

stdfs::path scratch(std::string_view what) {
    // Emscripten names no program (no `openkal.process'): random bytes there.
    std::string tag;
    if constexpr (os::is_emscripten) tag = os::random::hex(8).value_or("0");
    else tag = std::to_string(os::process::self()->id);
    const auto dir = stdfs::temp_directory_path() / std::format("mcpplibs-os-watch-{}-{}", what, tag);
    stdfs::remove_all(dir);
    stdfs::create_directories(dir);
    return dir;
}

bool names(const std::vector<stdfs::path>& got, std::initializer_list<stdfs::path> want) {
    return got.size() == want.size() && std::ranges::all_of(want, [&](const auto& w) {
        return std::ranges::find(got, w) != got.end();
    });
}

std::string shown(const std::vector<stdfs::path>& got) {
    std::string out;
    for (const auto& p : got) out += std::format("[{}]", p.generic_string());
    return out;
}

// A wait long enough for any system's notice, and for a look every poll interval.
constexpr auto kSettle = 2s;

}  // namespace

UTest watch_tests{"watch", [] {
    "a file written is told once, and nothing when nothing changed"_test = [] {
        const auto dir = scratch("file");
        const auto file = dir / "a.txt";
        expect(fatal(os::fs::write(file, "one").has_value()));
        auto w = os::fs::watcher::open();
        expect(fatal(w.has_value()));
        expect(w->add(file).has_value());
        expect(w->changed().empty()) << "nothing changed yet";
        expect(os::fs::write(file, "two!").has_value());
        const auto got = w->changed(kSettle);
        expect(names(got, {file})) << shown(got);
        expect(w->changed().empty()) << "told once";
        stdfs::remove_all(dir);
    };

    "a file saved by a rename over it is told by its name"_test = [] {
        const auto dir = scratch("rename");
        const auto file = dir / "view.ts";
        expect(fatal(os::fs::write(file, "export const a = 1;").has_value()));
        auto w = os::fs::watcher::open();
        expect(fatal(w.has_value()));
        expect(w->add(file).has_value());
        expect(os::fs::write_atomic(file, "export const a = 2;").has_value());
        auto got = w->changed(kSettle);
        expect(names(got, {file})) << shown(got);
        // And again, the name now naming another node.
        expect(os::fs::write_atomic(file, "export const a = 3;").has_value());
        got = w->changed(kSettle);
        expect(names(got, {file})) << shown(got);
        stdfs::remove_all(dir);
    };

    "a name is told when it is made and when it is removed"_test = [] {
        const auto dir = scratch("made");
        const auto file = dir / "later" / "theme.json";
        auto w = os::fs::watcher::open();
        expect(fatal(w.has_value()));
        expect(w->add(file).has_value()) << "a name that is not there, in a directory that is not either";
        expect(w->changed().empty());
        stdfs::create_directories(file.parent_path());
        expect(os::fs::write(file, "{}").has_value());
        auto got = w->changed(kSettle);
        expect(names(got, {file})) << shown(got);
        stdfs::remove(file);
        got = w->changed(kSettle);
        expect(names(got, {file})) << shown(got);
        stdfs::remove_all(dir);
    };

    "a directory is told of its entries, and another file's change tells nothing"_test = [] {
        const auto dir = scratch("dir");
        const auto watched = dir / "components";
        stdfs::create_directories(watched);
        const auto other = dir / "other.txt";
        expect(fatal(os::fs::write(other, "x").has_value()));
        auto w = os::fs::watcher::open();
        expect(fatal(w.has_value()));
        expect(w->add(watched).has_value());
        expect(os::fs::write(other, "changed").has_value());
        auto got = w->changed(300ms);
        expect(got.empty()) << "a file beside the directory is not one of its entries: " << shown(got);
        expect(os::fs::write(watched / "button.ts", "x").has_value());
        got = w->changed(kSettle);
        expect(names(got, {watched})) << shown(got);
        stdfs::remove_all(dir);
    };

    // A thread writes while the case waits: not a case on Emscripten, which
    // starts no thread without -pthread.
    if constexpr (!os::is_emscripten) "a wait ends when the change comes"_test = [] {
        const auto dir = scratch("wait");
        const auto file = dir / "app.ts";
        expect(fatal(os::fs::write(file, "1").has_value()));
        auto w = os::fs::watcher::open();
        expect(fatal(w.has_value()));
        expect(w->add(file).has_value());
        std::jthread writer([&] {
            std::this_thread::sleep_for(100ms);
            (void)os::fs::write(file, "22");
        });
        const auto begun = std::chrono::steady_clock::now();
        const auto got = w->changed(5s);
        const auto took = std::chrono::steady_clock::now() - begun;
        expect(names(got, {file})) << shown(got);
        expect(took < 2s) << std::chrono::duration_cast<std::chrono::milliseconds>(took).count() << " ms";
        stdfs::remove_all(dir);
    };

    "a system with notice is said so, and gives a handle where it has one"_test = [] {
        auto w = os::fs::watcher::open();
        expect(fatal(w.has_value()));
        const bool noticed = os::host() == os::platform::linux || os::host() == os::platform::macos
                          || os::host() == os::platform::windows;
        if constexpr (os::is_emscripten) expect(!w->notified());
        else if (noticed && !os::is_openkal) expect(w->notified());
        if (w->notified() && os::host() != os::platform::windows) expect(w->native_handle().has_value());
    };
}};
