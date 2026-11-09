// Linux's notice of change for `fs::watcher': inotify, by the kernel's calls
// (src/linux/kernel.h). A directory is watched for its entries and their
// content, which covers a file in it; a file needs nothing more. The watch of a
// directory removed is dropped by the kernel, and the watcher asks again of
// what its name names when it changes.
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

bool notifier_wait(notifier& n, std::chrono::milliseconds wait) {
    if (wait.count() > 0 && k::wait_readable(n.fd, static_cast<long>(wait.count())) <= 0) return false;
    // The records are read and dropped: the watcher looks at its names.
    alignas(8) char bytes[16384];
    bool any = false;
    while (k::read(n.fd, bytes, sizeof bytes) > 0) any = true;
    return any;
}

std::optional<std::intptr_t> notifier_handle(const notifier& n) noexcept { return n.fd; }

}  // namespace mcpplibs::os::fs::sys
