// macOS's notice of change for `fs::watcher': kqueue's notices of a node
// (EVFILT_VNODE), by the kernel's calls (src/macos/kernel.h). A directory's
// notice is of its entries; a file is watched itself for its content, and its
// directory for a file made, removed or renamed over it.
module;
#include "kernel.h"
#include "../posix/kernel.h"

module mcpplibs.os.fs;

import std;

namespace mcpplibs::os::fs::sys {

namespace k = mcpplibs::os::macos_kernel;

struct notifier {
    int queue = -1;
    struct node {
        std::string name;
        bool directory = false;
    };
    std::map<int, node> nodes;   // descriptor -> node
};

namespace {

constexpr unsigned kNotes = k::note_delete | k::note_write | k::note_extend | k::note_attrib | k::note_link
                          | k::note_rename | k::note_revoke;
constexpr unsigned kGone = k::note_delete | k::note_rename | k::note_revoke;

void forget(notifier& n, int fd) {
    mcpplibs::os::kernel::close(fd);
    n.nodes.erase(fd);
}

}  // namespace

notifier* notifier_open() {
    const long queue = k::kqueue();
    if (queue < 0) return nullptr;
    auto* n = new notifier;
    n->queue = static_cast<int>(queue);
    return n;
}

void notifier_close(notifier* n) noexcept {
    if (!n) return;
    for (const auto& [fd, node] : n->nodes) mcpplibs::os::kernel::close(fd);
    mcpplibs::os::kernel::close(n->queue);
    delete n;
}

bool notifier_watch(notifier& n, const std::string& node, bool directory) {
    notifier_unwatch(n, node);
    const long fd = k::open_events(node.c_str());
    if (fd < 0) return false;
    const k::kevent_t change{ static_cast<unsigned long>(fd), k::evfilt_vnode,
                              static_cast<unsigned short>(k::ev_add | k::ev_enable | k::ev_clear), kNotes, 0, nullptr };
    if (k::kevent(n.queue, &change, 1, nullptr, 0, 0) < 0) {
        mcpplibs::os::kernel::close(static_cast<int>(fd));
        return false;
    }
    n.nodes[static_cast<int>(fd)] = { node, directory };
    return true;
}

void notifier_unwatch(notifier& n, const std::string& node) {
    const auto it = std::ranges::find_if(n.nodes, [&](const auto& e) { return e.second.name == node; });
    if (it != n.nodes.end()) forget(n, it->first);
}

std::vector<notice> notifier_take(notifier& n, std::chrono::milliseconds wait) {
    std::vector<notice> out;
    std::array<k::kevent_t, 64> events{};
    const long got = k::kevent(n.queue, nullptr, 0, events.data(), static_cast<int>(events.size()),
                               static_cast<long>(wait.count()));
    for (long i = 0; i < got; ++i) {
        const int fd = static_cast<int>(events[static_cast<std::size_t>(i)].ident);
        const auto it = n.nodes.find(fd);
        if (it == n.nodes.end()) continue;
        out.push_back({ it->second.name, it->second.directory });
        // A node removed or renamed away is watched no more; the watcher asks
        // again of what its name names now.
        if (events[static_cast<std::size_t>(i)].fflags & kGone) forget(n, fd);
    }
    return out;
}

std::optional<std::intptr_t> notifier_handle(const notifier& n) noexcept { return n.queue; }

}  // namespace mcpplibs::os::fs::sys
