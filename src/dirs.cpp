module mcpplibs.os.dirs;

import std;
import mcpplibs.os.platform;
import mcpplibs.os.env;

namespace mcpplibs::os::dirs {

namespace {
std::optional<std::filesystem::path> var(std::string_view name) {
    auto v = env::get(name);
    if (!v || v->empty()) return std::nullopt;
    return std::filesystem::path(*v);
}
std::optional<std::filesystem::path> under_home(std::string_view rest) {
    auto h = home();
    if (!h) return std::nullopt;
    return *h / std::filesystem::path(rest);
}
bool windows() { return host() == platform::windows; }
bool macos() { return host() == platform::macos; }
}  // namespace

std::optional<std::filesystem::path> home() {
    return windows() ? var("USERPROFILE") : var("HOME");
}

std::optional<std::filesystem::path> config() {
    if (windows()) return var("APPDATA");
    if (macos()) return under_home("Library/Application Support");
    if (auto x = var("XDG_CONFIG_HOME")) return x;
    return under_home(".config");
}

std::optional<std::filesystem::path> data() {
    if (windows()) return var("LOCALAPPDATA");
    if (macos()) return under_home("Library/Application Support");
    if (auto x = var("XDG_DATA_HOME")) return x;
    return under_home(".local/share");
}

std::optional<std::filesystem::path> state() {
    if (windows()) return var("LOCALAPPDATA");
    if (macos()) return under_home("Library/Application Support");
    if (auto x = var("XDG_STATE_HOME")) return x;
    return under_home(".local/state");
}

std::optional<std::filesystem::path> cache() {
    if (windows()) return var("LOCALAPPDATA");
    if (macos()) return under_home("Library/Caches");
    if (auto x = var("XDG_CACHE_HOME")) return x;
    return under_home(".cache");
}

std::optional<std::filesystem::path> runtime() {
    if (!windows() && !macos())
        if (auto x = var("XDG_RUNTIME_DIR")) return x;
    return temp();
}

std::filesystem::path temp() {
    if (windows()) {
        if (auto x = var("TEMP")) return *x;
        if (auto x = var("TMP")) return *x;
        if (auto h = home()) return *h / "AppData" / "Local" / "Temp";
        return std::filesystem::path("C:/Windows/Temp");
    }
    if (auto x = var("TMPDIR")) return *x;
    return std::filesystem::path("/tmp");
}

}  // namespace mcpplibs::os::dirs
