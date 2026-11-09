// No notice of change: an ok image, whose system is openkal's and names no
// watch, and Emscripten. `fs::watcher' then looks when asked.
module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

struct notifier {};

notifier* notifier_open() { return nullptr; }
void notifier_close(notifier*) noexcept {}
bool notifier_watch(notifier&, const std::string&, bool) { return false; }
void notifier_unwatch(notifier&, const std::string&) {}
std::vector<notice> notifier_take(notifier&, std::chrono::milliseconds) { return {}; }
std::optional<std::intptr_t> notifier_handle(const notifier&) noexcept { return std::nullopt; }

}  // namespace mcpplibs::os::fs::sys
