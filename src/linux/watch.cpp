// Linux's notice of change for `fs::watcher': inotify, by the kernel's calls
// (src/linux/kernel.h). A directory is watched for its entries and their
// content, which covers a file in it; a file needs nothing more.
module;
#include "kernel.h"
#include "../posix/kernel.h"

module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

namespace k = mcpplibs::os::linux_kernel;

struct notifier {
    int fd = -1;
    std::map<int, std::string> directories;   // watch -> directory
};

namespace {

constexpr unsigned kMask = k::in_modify | k::in_attrib | k::in_close_write | k::in_moved_from | k::in_moved_to
                         | k::in_create | k::in_delete | k::in_delete_self | k::in_move_self | k::in_onlydir;
constexpr unsigned kEntries = k::in_moved_from | k::in_moved_to | k::in_create | k::in_delete;

// The records of one read: `struct inotify_event' (watch, mask, cookie, length)
// and the name, padded, after each.
void parse(notifier& n, const char* bytes, long count, std::vector<notice>& out) {
    long at = 0;
    while (at + 16 <= count) {
        int watch;
        unsigned mask, length;
        std::memcpy(&watch, bytes + at, 4);
        std::memcpy(&mask, bytes + at + 4, 4);
        std::memcpy(&length, bytes + at + 12, 4);
        // The name is padded with NULs to `length'; none when `length' is 0.
        std::string_view name(bytes + at + 16, length);
        name = name.substr(0, name.find('\0'));
        at += 16 + static_cast<long>(length);
        if (mask & k::in_q_overflow) {
            for (const auto& [w, directory] : n.directories) out.push_back({ directory, true });
            continue;
        }
        const auto it = n.directories.find(watch);
        if (it == n.directories.end()) continue;
        const std::string directory = it->second;
        if (mask & k::in_ignored) n.directories.erase(it);
        if (mask & (k::in_delete_self | k::in_move_self)) out.push_back({ directory, true });
        if (name.empty()) continue;
        const std::string entry = directory.ends_with('/') ? directory + std::string(name) : directory + "/" + std::string(name);
        out.push_back({ entry, false });
        if (mask & kEntries) out.push_back({ directory, true });
    }
}

}  // namespace

notifier* notifier_open() {
    const long fd = k::inotify_open();
    if (fd < 0) return nullptr;
    auto* n = new notifier;
    n->fd = static_cast<int>(fd);
    return n;
}

void notifier_close(notifier* n) noexcept {
    if (!n) return;
    mcpplibs::os::kernel::close(n->fd);
    delete n;
}

bool notifier_watch(notifier& n, const std::string& node, bool directory) {
    if (!directory) return true;
    const long watch = k::inotify_watch(n.fd, node.c_str(), kMask);
    if (watch < 0) return false;
    n.directories[static_cast<int>(watch)] = node;
    return true;
}

void notifier_unwatch(notifier& n, const std::string& node) {
    const auto it = std::ranges::find_if(n.directories, [&](const auto& w) { return w.second == node; });
    if (it == n.directories.end()) return;
    k::inotify_unwatch(n.fd, it->first);
    n.directories.erase(it);
}

std::vector<notice> notifier_take(notifier& n, std::chrono::milliseconds wait) {
    std::vector<notice> out;
    if (wait.count() > 0 && k::wait_readable(n.fd, static_cast<long>(wait.count())) <= 0) return out;
    alignas(8) char bytes[16384];
    for (long got = k::read(n.fd, bytes, sizeof bytes); got > 0; got = k::read(n.fd, bytes, sizeof bytes))
        parse(n, bytes, got, out);
    return out;
}

std::optional<std::intptr_t> notifier_handle(const notifier& n) noexcept { return n.fd; }

}  // namespace mcpplibs::os::fs::sys
