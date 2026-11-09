// Windows's notice of change for `fs::watcher': a directory's changes read
// overlapped (ReadDirectoryChangesW), one event per directory. A directory is
// watched for its entries and their content, which covers a file in it; a file
// needs nothing more.
module;
#include "win32.h"

module mcpplibs.os.fs;

import std;

#include "text.h"

namespace mcpplibs::os::fs::sys {

struct notifier {
    struct directory {
        std::string name;
        okos_handle handle = nullptr;
        okos_handle event = nullptr;
        okos_overlapped overlapped{};
        std::vector<std::uint32_t> buffer = std::vector<std::uint32_t>(16384);   // 64 KiB, DWORD-aligned
    };
    std::list<directory> directories;   // stable: the system writes into each one's buffer
};

namespace {

// FILE_NOTIFY_CHANGE_FILE_NAME | DIR_NAME | ATTRIBUTES | SIZE | LAST_WRITE.
constexpr okos_dword kFilter = 0x1 | 0x2 | 0x4 | 0x8 | 0x10;
constexpr okos_dword kIncomplete = 996, kTimeout = 0x102;

okos_handle invalid() { return reinterpret_cast<okos_handle>(static_cast<std::intptr_t>(-1)); }

bool request(notifier::directory& d) {
    late::ResetEvent(d.event);
    d.overlapped = okos_overlapped{};
    d.overlapped.event = d.event;
    return late::ReadDirectoryChangesW(d.handle, d.buffer.data(), static_cast<okos_dword>(d.buffer.size() * 4), 0,
                                       kFilter, nullptr, &d.overlapped, nullptr) != 0;
}

void close(notifier::directory& d) {
    late::CancelIoEx(d.handle, &d.overlapped);
    okos_dword bytes = 0;
    late::GetOverlappedResult(d.handle, &d.overlapped, &bytes, 1);
    CloseHandle(d.handle);
    CloseHandle(d.event);
}

// FILE_NOTIFY_INFORMATION records: the offset of the next, the action, the
// name's length in bytes, the name in UTF-16 relative to the directory.
void parse(const notifier::directory& d, okos_dword bytes, std::vector<notice>& out) {
    if (bytes == 0) {   // more changed than the buffer held: every entry may have
        out.push_back({ d.name, true });
        return;
    }
    const auto* base = reinterpret_cast<const unsigned char*>(d.buffer.data());
    for (okos_dword at = 0;;) {
        okos_dword next, action, length;
        std::memcpy(&next, base + at, 4);
        std::memcpy(&action, base + at + 4, 4);
        std::memcpy(&length, base + at + 8, 4);
        std::u16string name(length / 2, u'\0');
        std::memcpy(name.data(), base + at + 12, length);
        std::string entry = utf8_from_utf16(name);
        std::ranges::replace(entry, '\\', '/');
        out.push_back({ d.name.ends_with('/') ? d.name + entry : d.name + "/" + entry, false });
        if (action != 3 /* MODIFIED */) out.push_back({ d.name, true });
        if (next == 0) break;
        at += next;
    }
}

// What the directories completed: each read, reissued.
void harvest(notifier& n, std::vector<notice>& out) {
    for (auto& d : n.directories) {
        okos_dword bytes = 0;
        if (!late::GetOverlappedResult(d.handle, &d.overlapped, &bytes, 0)) {
            if (GetLastError() == kIncomplete) continue;
            out.push_back({ d.name, true });   // the directory went away, or its read failed
        } else {
            parse(d, bytes, out);
        }
        request(d);
    }
}

}  // namespace

notifier* notifier_open() { return new notifier; }

void notifier_close(notifier* n) noexcept {
    if (!n) return;
    for (auto& d : n->directories) close(d);
    delete n;
}

bool notifier_watch(notifier& n, const std::string& node, bool directory) {
    if (!directory) return true;
    notifier_unwatch(n, node);
    // FILE_LIST_DIRECTORY; shared every way; OPEN_EXISTING; BACKUP_SEMANTICS | OVERLAPPED.
    const okos_handle h = CreateFileW(utf16_from_utf8(node).c_str(), 0x1, 0x7, nullptr, 3, 0x02000000 | 0x40000000, nullptr);
    if (h == invalid() || h == nullptr) return false;
    auto& d = n.directories.emplace_back();
    d.name = node;
    d.handle = h;
    d.event = late::CreateEventW(nullptr, 1, 0, nullptr);
    if (!d.event || !request(d)) {
        CloseHandle(h);
        if (d.event) CloseHandle(d.event);
        n.directories.pop_back();
        return false;
    }
    return true;
}

void notifier_unwatch(notifier& n, const std::string& node) {
    const auto it = std::ranges::find_if(n.directories, [&](const auto& d) { return d.name == node; });
    if (it == n.directories.end()) return;
    close(*it);
    n.directories.erase(it);
}

std::vector<notice> notifier_take(notifier& n, std::chrono::milliseconds wait) {
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + wait;
    std::vector<notice> out;
    for (;;) {
        harvest(n, out);
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now());
        if (!out.empty() || left.count() <= 0 || n.directories.empty()) return out;
        // One wait upon every directory's event where the system takes them all
        // (64); beyond, a look every few milliseconds.
        if (n.directories.size() <= 64) {
            std::vector<okos_handle> events;
            for (const auto& d : n.directories) events.push_back(d.event);
            if (late::WaitForMultipleObjects(static_cast<okos_dword>(events.size()), events.data(), 0,
                                       static_cast<okos_dword>(left.count())) == kTimeout)
                return out;
        } else {
            std::this_thread::sleep_for(std::min(left, std::chrono::milliseconds{10}));
        }
    }
}

std::optional<std::intptr_t> notifier_handle(const notifier&) noexcept { return std::nullopt; }

}  // namespace mcpplibs::os::fs::sys
