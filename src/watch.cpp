// mcpplibs.os.fs: the watcher, the same on every system. What a system adds is
// its notice (`sys::notifier', src/<os>/watch.cpp); what changed is decided
// here, from each name's state, so every system answers alike.
module mcpplibs.os.fs;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::fs {

namespace {

// What a name is now: whether it is there, a directory or not, its stamp and
// the node it names. Two states differ exactly when the name changed in a way
// a watcher tells.
struct snapshot {
    bool exists = false;
    bool directory = false;
    stamp st{};
    identity id{};
    bool operator==(const snapshot&) const = default;
};

snapshot snapshot_of(const std::string& name) {
    snapshot s;
    std::error_code ec;
    const auto path = from_utf8(name);
    const auto status = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::exists(status)) return s;
    s.exists = true;
    s.directory = std::filesystem::is_directory(status);
    if (auto st = stamp_of(path)) s.st = *st;
    if (auto id = identity_of(path)) s.id = *id;
    return s;
}

std::string parent_of(const std::string& name) {
    const auto slash = name.rfind('/');
    if (slash == std::string::npos) return name;
    // The root keeps its separator (`/', `C:/').
    if (slash == 0 || (slash > 0 && name[slash - 1] == ':')) return name.substr(0, slash + 1);
    return name.substr(0, slash);
}

// The nearest directory that exists: the name's own, or the closest above it.
std::string existing_directory(std::string name) {
    std::error_code ec;
    while (!std::filesystem::is_directory(from_utf8(name), ec)) {
        const auto up = parent_of(name);
        if (up == name) break;
        name = up;
    }
    return name;
}

}  // namespace

struct watcher::state {
    struct name {
        std::filesystem::path given;   // as added: what `changed' answers
        std::string key;               // absolute, generic
        snapshot last;
        std::vector<std::string> armed;   // the nodes notices are asked of for it
    };
    sys::notifier* notifier = nullptr;
    std::vector<name> names;

    ~state() { if (notifier) sys::notifier_close(notifier); }

    // Notices of the name from now: its directory --- the nearest that exists
    // --- for a name made, removed or renamed over, and the name itself.
    void arm(name& n) {
        if (!notifier) return;
        std::vector<std::string> nodes;
        nodes.push_back(existing_directory(parent_of(n.key)));
        if (n.last.exists) nodes.push_back(n.key);
        for (const auto& node : n.armed)
            if (std::ranges::find(nodes, node) == nodes.end()) sys::notifier_unwatch(*notifier, node);
        for (const auto& node : nodes) {
            std::error_code ec;
            sys::notifier_watch(*notifier, node, std::filesystem::is_directory(from_utf8(node), ec));
        }
        n.armed = std::move(nodes);
    }

    // Whether a notice is about the name: it names it, or --- for a directory
    // --- its entries changed.
    static bool told(const name& n, const std::vector<sys::notice>& notices) {
        return std::ranges::any_of(notices, [&](const sys::notice& x) {
            return same(from_utf8(x.name), from_utf8(n.key)) && (x.entries || !n.last.directory || !n.last.exists);
        });
    }

    std::vector<std::filesystem::path> evaluate(const std::vector<sys::notice>& notices) {
        std::vector<std::filesystem::path> out;
        for (auto& n : names) {
            const auto now = snapshot_of(n.key);
            const bool changed = now != n.last || told(n, notices);
            const bool node_changed = now.exists != n.last.exists || now.id != n.last.id;
            n.last = now;
            if (!changed) continue;
            out.push_back(n.given);
            if (node_changed) arm(n);
        }
        return out;
    }
};

watcher::watcher(std::unique_ptr<state> s) noexcept : s_(std::move(s)) {}
watcher::watcher(watcher&&) noexcept = default;
watcher& watcher::operator=(watcher&&) noexcept = default;
watcher::~watcher() = default;

result<watcher> watcher::open() {
    auto s = std::make_unique<state>();
    s->notifier = sys::notifier_open();
    return watcher(std::move(s));
}

result<void> watcher::add(const std::filesystem::path& name) {
    const std::string key = generic(fs::absolute(name));
    if (std::ranges::any_of(s_->names, [&](const auto& n) { return same(from_utf8(n.key), from_utf8(key)); }))
        return {};
    state::name n{ name, key, snapshot_of(key), {} };
    s_->arm(n);
    s_->names.push_back(std::move(n));
    return {};
}

void watcher::remove(const std::filesystem::path& name) {
    const std::string key = generic(fs::absolute(name));
    auto it = std::ranges::find_if(s_->names, [&](const auto& n) { return same(from_utf8(n.key), from_utf8(key)); });
    if (it == s_->names.end()) return;
    // A node another name still asks of stays watched.
    for (const auto& node : it->armed) {
        const bool shared = std::ranges::any_of(s_->names, [&](const auto& other) {
            return &other != &*it && std::ranges::find(other.armed, node) != other.armed.end();
        });
        if (!shared && s_->notifier) sys::notifier_unwatch(*s_->notifier, node);
    }
    s_->names.erase(it);
}

std::vector<std::filesystem::path> watcher::changed(std::chrono::milliseconds wait) {
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + wait;
    auto remaining = [&] {
        return std::max(std::chrono::milliseconds{0},
                        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()));
    };
    // What is there now, then what arrives: a notice of another name in a
    // watched directory is no change of these, and the wait goes on.
    auto out = s_->evaluate(s_->notifier ? sys::notifier_take(*s_->notifier, std::chrono::milliseconds{0})
                                         : std::vector<sys::notice>{});
    while (out.empty() && remaining() > std::chrono::milliseconds{0}) {
        if (s_->notifier) {
            out = s_->evaluate(sys::notifier_take(*s_->notifier, remaining()));
        } else {
            std::this_thread::sleep_for(std::min(kPollInterval, remaining()));
            out = s_->evaluate({});
        }
    }
    return out;
}

bool watcher::notified() const noexcept { return s_->notifier != nullptr; }

std::optional<std::intptr_t> watcher::native_handle() const noexcept {
    return s_->notifier ? sys::notifier_handle(*s_->notifier) : std::nullopt;
}

}  // namespace mcpplibs::os::fs
