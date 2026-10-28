module mcpplibs.os.fs;

import std;
import openkal.types;
import openkal.fs;
import openkal.stream;
import openkal.random;
import mcpplibs.os.platform;
import mcpplibs.os.error;

namespace mcpplibs::os::fs {

namespace {

struct supplied { kal_dir dir; std::string name; };

// The supplied directories. Read once and read again when the count changes:
// an environment may append one while the program runs (openkal 0.17, a volume
// mounted since), and a position once given keeps its directory.
std::mutex g_table_lock;
std::vector<supplied> g_table;

std::vector<supplied> table() {
    std::lock_guard guard(g_table_lock);
    const kal_uintptr n = kal_fs_preopen_count();
    for (kal_uintptr i = g_table.size(); i < n; ++i) {
        kal_dir d{};
        char name[1024];
        kal_uintptr len = 0;
        if (kal_fs_preopen(i, &d, name, sizeof name, &len) != kal_ok || len > sizeof name) {
            g_table.push_back(supplied{ kal_dir{}, {} });
            continue;
        }
        g_table.push_back(supplied{ d, std::string(name, len) });
    }
    return g_table;
}

bool separator(char c) { return c == '/' || c == '\\'; }

std::string slashed(std::string s) {
    for (auto& c : s) if (c == '\\') c = '/';
    while (s.size() > 1 && s.back() == '/' && !(s.size() == 2 && s[0] == '/')) s.pop_back();
    return s;
}

bool same_prefix(std::string_view path, std::string_view prefix, bool fold) {
    if (path.size() < prefix.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        char a = path[i], b = prefix[i];
        if (fold) {
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) return false;
    }
    return true;
}

struct opened_file {
    kal_file f{};
    ~opened_file() { if (f.h) kal_fs_close_file(f); }
};

result<located> where(const std::filesystem::path& p, std::string_view doing) {
    auto l = locate(p);
    if (!l) return std::unexpected(error{ errc::not_found,
        std::format("{} {}: beneath no directory this program was given", doing, generic_utf8(p)) });
    return *l;
}

kal_dir dir_of(const located& l) { return kal_dir{ static_cast<kal_uintptr>(l.directory) }; }

// The parent's place and the last component, for operations upon a name in a
// directory the caller may not have opened.
std::string parent_of(const std::string& relative) {
    const auto at = relative.find_last_of('/');
    return at == std::string::npos ? std::string(".") : relative.substr(0, at);
}

std::string unique_suffix() {
    unsigned char bytes[8] = {};
    kal_random_fill(bytes, sizeof bytes);
    std::string s = ".tmp-";
    for (auto b : bytes) s += std::format("{:02x}", b);
    return s;
}

result<void> write_all(kal_file f, std::string_view bytes, std::string_view doing) {
    const kal_stream s = kal_fs_stream(f);
    std::size_t done = 0;
    while (done < bytes.size()) {
        const kal_intptr n = kal_stream_write(s, bytes.data() + done, bytes.size() - done);
        if (n < 0) return std::unexpected(from_kal(static_cast<int>(-n), doing));
        if (n == 0) return std::unexpected(error{ errc::io, std::string(doing) });
        done += static_cast<std::size_t>(n);
    }
    return {};
}

}  // namespace

std::string utf8(const std::filesystem::path& name) {
    const std::u8string u = name.u8string();
    return std::string(u.begin(), u.end());
}

std::string generic_utf8(const std::filesystem::path& name) {
    const std::u8string u = name.generic_u8string();
    return std::string(u.begin(), u.end());
}

std::filesystem::path from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::filesystem::path absolute(const std::filesystem::path& name) {
    const std::string s = generic_utf8(name);
    const bool windows = host() == platform::windows;
    const bool already = windows
        ? (s.size() >= 3 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' && separator(s[2]))
              || (s.size() >= 2 && separator(s[0]) && separator(s[1]))
        : !s.empty() && s[0] == '/';
    if (already) return name;
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (ec) return name;
    return from_utf8(slashed(generic_utf8(cwd)) + "/" + s);
}

namespace {
// The root a name begins with, in the generic spelling: `C:/' or `C:' (a drive
// and the directory current on it), `//server/share/' on Windows; `/' elsewhere;
// empty for a relative name.
std::size_t root_length(std::string_view g, bool windows) {
    if (windows) {
        if (g.size() >= 2 && std::isalpha(static_cast<unsigned char>(g[0])) && g[1] == ':')
            return g.size() >= 3 && g[2] == '/' ? 3 : 2;
        if (g.size() >= 2 && g[0] == '/' && g[1] == '/') {
            const auto server = g.find('/', 2);
            if (server == std::string_view::npos) return g.size();
            const auto share = g.find('/', server + 1);
            return share == std::string_view::npos ? g.size() : share + 1;
        }
    }
    return !g.empty() && g[0] == '/' ? 1 : 0;
}

bool fold_equal(std::string_view a, std::string_view b, bool fold) {
    return a.size() == b.size() && same_prefix(a, b, fold);
}
}  // namespace

std::string generic(const std::filesystem::path& name) {
    const bool windows = host() == platform::windows;
    std::string s = generic_utf8(name);
    if (windows) std::ranges::replace(s, '\\', '/');
    const std::size_t rootLen = root_length(s, windows);
    const std::string root = s.substr(0, rootLen);
    std::vector<std::string_view> parts;
    std::string_view rest = std::string_view(s).substr(rootLen);
    while (!rest.empty()) {
        const auto slash = rest.find('/');
        const auto part = rest.substr(0, slash);
        rest = slash == std::string_view::npos ? std::string_view{} : rest.substr(slash + 1);
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..") { parts.pop_back(); continue; }
            if (!root.empty()) continue;               // nothing is above a root
        }
        parts.push_back(part);
    }
    std::string out = root;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += '/';
        out += parts[i];
    }
    if (out.empty()) out = ".";
    return out;
}

std::string native(const std::filesystem::path& name) {
    std::string g = generic(name);
    if (host() == platform::windows) std::ranges::replace(g, '/', '\\');
    return g;
}

bool is_absolute(const std::filesystem::path& name) {
    const bool windows = host() == platform::windows;
    const std::string g = generic(name);
    const auto r = root_length(g, windows);
    // `C:x' names a place relative to the drive's current directory: not absolute.
    return windows ? r >= 3 || (r > 0 && g[0] == '/' && g.size() > 1 && g[1] == '/') : r == 1;
}

bool same(const std::filesystem::path& a, const std::filesystem::path& b) {
    return fold_equal(generic(a), generic(b), conventions().case_insensitive_names);
}

bool within(const std::filesystem::path& name, const std::filesystem::path& directory) {
    const bool fold = conventions().case_insensitive_names;
    const std::string n = generic(name), d = generic(directory);
    if (fold_equal(n, d, fold)) return true;
    if (!same_prefix(n, d, fold)) return false;
    return d.back() == '/' || n[d.size()] == '/';
}

bool names_a_place(std::string_view name) {
    if (host() == platform::windows)
        return name.find_first_of("/\\") != std::string_view::npos
            || (name.size() >= 2 && std::isalpha(static_cast<unsigned char>(name[0])) && name[1] == ':');
    return name.find('/') != std::string_view::npos;
}

std::optional<located> locate(const std::filesystem::path& name) {
    const auto whole = mcpplibs::os::fs::absolute(name);
    const std::string path = slashed(generic_utf8(whole.lexically_normal()));
    const bool fold = conventions().case_insensitive_names;
    std::optional<located> best;
    std::size_t best_len = 0;
    for (const auto& s : table()) {
        if (!s.dir.h || s.name.empty()) continue;
        const std::string prefix = slashed(s.name);
        if (!same_prefix(path, prefix, fold)) continue;
        const bool root = separator(prefix.back());
        if (!root && path.size() > prefix.size() && !separator(path[prefix.size()])) continue;
        if (best && best_len >= prefix.size()) continue;
        std::string_view rest = std::string_view(path).substr(prefix.size());
        while (!rest.empty() && separator(rest.front())) rest.remove_prefix(1);
        best = located{ static_cast<std::uintptr_t>(s.dir.h), s.name,
                        rest.empty() ? std::string(".") : std::string(rest) };
        best_len = prefix.size();
    }
    return best;
}

std::vector<std::string> preopens() {
    std::vector<std::string> names;
    for (const auto& s : table()) names.push_back(s.name);
    return names;
}

namespace {
// The whole file, or past `at_most' errc::too_large: each read asks for no more
// than one byte beyond the bound.
result<std::string> read_up_to(const std::filesystem::path& file, std::optional<std::size_t> at_most) {
    auto l = where(file, "reading");
    if (!l) return std::unexpected(l.error());
    opened_file f;
    const int rc = kal_fs_open(dir_of(*l), l->relative.data(), l->relative.size(),
                               kal::fs::open::read.bits, &f.f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("reading {}", generic_utf8(file))));
    // Read into the string's own storage, not through a buffer on the stack:
    // a 64 KiB array is all of a wasm module's default stack (Emscripten's
    // 65536 bytes, where `read' overflowed it) and a large part of a small
    // thread's.
    constexpr std::size_t chunk = 65536;
    std::string out;
    for (;;) {
        std::size_t want = chunk;
        if (at_most) want = std::min(want, *at_most + 1 - out.size());
        const std::size_t at = out.size();
        kal_intptr n = 0;
        out.resize_and_overwrite(at + want, [&](char* p, std::size_t) {
            n = kal_stream_read(kal_fs_stream(f.f), p + at, want);
            return at + (n > 0 ? static_cast<std::size_t>(n) : 0);
        });
        if (n < 0) return std::unexpected(from_kal(static_cast<int>(-n), std::format("reading {}", generic_utf8(file))));
        if (n == 0) break;
        if (at_most && out.size() > *at_most)
            return std::unexpected(error{ errc::too_large, std::format("reading {} (more than {} bytes)", generic_utf8(file), *at_most) });
    }
    return out;
}
}  // namespace

result<std::string> read(const std::filesystem::path& file) { return read_up_to(file, std::nullopt); }

result<std::string> read(const std::filesystem::path& file, std::size_t at_most) { return read_up_to(file, at_most); }

result<void> write(const std::filesystem::path& file, std::string_view bytes) {
    auto l = where(file, "writing");
    if (!l) return std::unexpected(l.error());
    opened_file f;
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::truncate;
    const int rc = kal_fs_open(dir_of(*l), l->relative.data(), l->relative.size(), flags.bits, &f.f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("writing {}", generic_utf8(file))));
    return write_all(f.f, bytes, std::format("writing {}", generic_utf8(file)));
}

namespace {
// Written beside the name and made durable; the caller renames it.
result<std::string> write_beside(const located& l, std::string_view bytes, std::string_view doing) {
    const std::string temp = l.relative + unique_suffix();
    opened_file f;
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::exclusive;
    int rc = kal_fs_open(dir_of(l), temp.data(), temp.size(), flags.bits, &f.f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    if (auto w = write_all(f.f, bytes, doing); !w) {
        kal_fs_close_file(f.f); f.f = {};
        kal_fs_remove(dir_of(l), temp.data(), temp.size());
        return std::unexpected(w.error());
    }
    rc = kal_stream_flush(kal_fs_stream(f.f));
    kal_fs_close_file(f.f); f.f = {};
    if (rc != kal_ok) {
        kal_fs_remove(dir_of(l), temp.data(), temp.size());
        return std::unexpected(from_kal(rc, doing));
    }
    return temp;
}
}  // namespace

result<void> write_atomic(const std::filesystem::path& file, std::string_view bytes) {
    const std::string doing = std::format("writing {}", generic_utf8(file));
    auto l = where(file, "writing");
    if (!l) return std::unexpected(l.error());
    auto temp = write_beside(*l, bytes, doing);
    if (!temp) return std::unexpected(temp.error());
    const int rc = kal_fs_rename(dir_of(*l), temp->data(), temp->size(),
                                 dir_of(*l), l->relative.data(), l->relative.size());
    if (rc != kal_ok) {
        kal_fs_remove(dir_of(*l), temp->data(), temp->size());
        return std::unexpected(from_kal(rc, doing));
    }
    return {};
}

result<published> publish(const std::filesystem::path& file, std::string_view bytes) {
    const std::string doing = std::format("publishing {}", generic_utf8(file));
    auto l = where(file, "publishing");
    if (!l) return std::unexpected(l.error());
    kal_dir parent{};
    const std::string up = parent_of(l->relative);
    bool close_parent = false;
    if (up == ".") parent = dir_of(*l);
    else if (kal_fs_open_dir(dir_of(*l), up.data(), up.size(), &parent) == kal_ok) close_parent = true;
    const bool atomic = parent.h && (kal_fs_props(parent) & kal::fs::rename_no_replace.bits) != 0;
    if (close_parent) kal_fs_close_dir(parent);

    if (atomic) {
        auto temp = write_beside(*l, bytes, doing);
        if (!temp) return std::unexpected(temp.error());
        const int rc = kal_fs_rename_flags(dir_of(*l), temp->data(), temp->size(),
                                           dir_of(*l), l->relative.data(), l->relative.size(),
                                           kal::fs::no_replace.bits);
        if (rc != kal_ok) {
            kal_fs_remove(dir_of(*l), temp->data(), temp->size());
            return std::unexpected(from_kal(rc, doing));
        }
        return published{ true };
    }
    // A volume that cannot rename without replacing: the name is taken
    // exclusively, which is atomic everywhere, and then written.
    opened_file f;
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::exclusive;
    const int rc = kal_fs_open(dir_of(*l), l->relative.data(), l->relative.size(), flags.bits, &f.f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    if (auto w = write_all(f.f, bytes, doing); !w) return std::unexpected(w.error());
    if (const int fl = kal_stream_flush(kal_fs_stream(f.f)); fl != kal_ok)
        return std::unexpected(from_kal(fl, doing));
    return published{ false };
}

result<capabilities> caps(const std::filesystem::path& directory) {
    auto l = where(directory, "asking about");
    if (!l) return std::unexpected(l.error());
    kal_dir d = dir_of(*l);
    bool close = false;
    if (l->relative != ".") {
        const int rc = kal_fs_open_dir(dir_of(*l), l->relative.data(), l->relative.size(), &d);
        if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("asking about {}", generic_utf8(directory))));
        close = true;
    }
    const kal_uintptr p = kal_fs_props(d);
    if (close) kal_fs_close_dir(d);
    capabilities c;
    c.case_sensitive    = (p & kal::fs::case_sensitive.bits) != 0;
    c.links             = (p & kal::fs::links.bits) != 0;
    c.make_links        = (p & kal::fs::make_links.bits) != 0;
    c.rename_no_replace = (p & kal::fs::rename_no_replace.bits) != 0;
    c.local_endpoint    = (p & kal::fs::local_endpoint.bits) != 0;
    c.locks             = (p & kal::fs::locks.bits) != 0;
    c.executable        = (p & kal::fs::executable.bits) != 0;
    c.atomic_rename     = (p & kal::fs::atomic_rename.bits) != 0;
    c.remove_open       = (p & kal::fs::remove_open.bits) != 0;
    return c;
}

result<lock> lock::acquire(const std::filesystem::path& file, kind k, bool wait) {
    const std::string doing = std::format("locking {}", generic_utf8(file));
    auto l = where(file, "locking");
    if (!l) return std::unexpected(l.error());
    kal_file f{};
    const auto flags = kal::fs::open::read | kal::fs::open::write | kal::fs::open::create;
    int rc = kal_fs_open(dir_of(*l), l->relative.data(), l->relative.size(), flags.bits, &f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    auto mode = (k == kind::exclusive ? kal::fs::lock::exclusive : kal::fs::lock::shared);
    if (wait) mode = mode | kal::fs::lock::wait;
    // The whole of the file, now and as it grows: a length of zero is "to the
    // end, wherever that comes to be".
    rc = kal_fs_lock(f, 0, 0, mode.bits);
    if (rc != kal_ok) { kal_fs_close_file(f); return std::unexpected(from_kal(rc, doing)); }
    lock out;
    out.file_ = static_cast<std::uintptr_t>(f.h);
    return out;
}

lock::lock(lock&& o) noexcept : file_(std::exchange(o.file_, 0)) {}
lock& lock::operator=(lock&& o) noexcept {
    if (this != &o) { release(); file_ = std::exchange(o.file_, 0); }
    return *this;
}
lock::~lock() { release(); }
void lock::release() noexcept {
    if (!file_) return;
    const kal_file f{ static_cast<kal_uintptr>(file_) };
    kal_fs_unlock(f, 0, 0);
    kal_fs_close_file(f);
    file_ = 0;
}

result<void> create_link(const std::filesystem::path& target, const std::filesystem::path& link,
                         bool directory) {
    const std::string doing = std::format("linking {} to {}", generic_utf8(link), generic_utf8(target));
    auto l = where(link, "linking");
    if (!l) return std::unexpected(l.error());
    const std::string t = generic_utf8(target);
    const int rc = kal_fs_link_create(dir_of(*l), l->relative.data(), l->relative.size(), t.data(), t.size(),
                                      directory ? kal::fs::link_directory : 0);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    return {};
}

result<std::string> read_link(const std::filesystem::path& link) {
    const std::string doing = std::format("reading the link {}", generic_utf8(link));
    auto l = where(link, "reading the link");
    if (!l) return std::unexpected(l.error());
    std::string out(4096, '\0');
    for (;;) {
        const kal_intptr n = kal_fs_link_read(dir_of(*l), l->relative.data(), l->relative.size(),
                                              out.data(), out.size());
        if (n < 0) return std::unexpected(from_kal(static_cast<int>(-n), doing));
        if (static_cast<std::size_t>(n) <= out.size()) { out.resize(static_cast<std::size_t>(n)); return out; }
        out.resize(static_cast<std::size_t>(n));
    }
}

result<void> set_executable(const std::filesystem::path& file, bool executable) {
    auto l = where(file, "marking");
    if (!l) return std::unexpected(l.error());
    const int rc = kal_fs_set_executable_at(dir_of(*l), l->relative.data(), l->relative.size(), executable ? 1 : 0);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("marking {}", generic_utf8(file))));
    return {};
}

namespace {
result<kal_node_info> info(const std::filesystem::path& node, kal_u32 wanted, std::string_view doing) {
    auto l = where(node, doing);
    if (!l) return std::unexpected(l.error());
    kal_node_info i{};
    i.self_size = sizeof i;
    // The kind is asked for as well, because absence is read from it: an
    // implementation fills only the fields it was asked for (openkal-emscripten
    // does), and a kind left at zero reads as kal_node_absent.
    const int rc = kal_fs_info(dir_of(*l), l->relative.data(), l->relative.size(), 0,
                               wanted | kal::fs::field::kind, &i);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("{} {}", doing, generic_utf8(node))));
    if (i.kind == kal_node_absent) return std::unexpected(error{ errc::not_found, std::format("{} {}", doing, generic_utf8(node)) });
    return i;
}
}  // namespace

result<stamp> stamp_of(const std::filesystem::path& node) {
    auto i = info(node, kal::fs::field::size | kal::fs::field::modified, "asking about");
    if (!i) return std::unexpected(i.error());
    return stamp{ i->size, i->modified_ns };
}

result<identity> identity_of(const std::filesystem::path& node) {
    auto i = info(node, kal::fs::field::identity, "asking about");
    if (!i) return std::unexpected(i.error());
    if ((i->present & kal::fs::field::identity) == 0)
        return std::unexpected(error{ errc::not_supported, std::format("the identity of {}", generic_utf8(node)) });
    return identity{ i->identity[0], i->identity[1] };
}

std::filesystem::path canonical(const std::filesystem::path& name) {
    const std::string whole = generic(fs::absolute(name));
    if (host() == platform::windows) {
        // A short alias always carries a `~'. A component that does is looked
        // up among its parent's entries, which are listed by their long names,
        // and replaced by the entry that is the same node.
        const std::size_t rootLen = root_length(whole, true);
        std::string current = whole.substr(0, rootLen);
        std::string_view rest = std::string_view(whole).substr(rootLen);
        while (!rest.empty()) {
            const auto slash = rest.find('/');
            const std::string part(rest.substr(0, slash));
            rest = slash == std::string_view::npos ? std::string_view{} : rest.substr(slash + 1);
            if (part.empty()) continue;
            const std::string joint = current.empty() || current.ends_with('/') ? "" : "/";
            std::string next = current + joint + part;
            if (part.find('~') != std::string::npos) {
                if (const auto id = identity_of(from_utf8(next))) {
                    std::error_code ec;
                    for (std::filesystem::directory_iterator it(from_utf8(current), ec), end; !ec && it != end; it.increment(ec)) {
                        const std::string entry = generic_utf8(it->path().filename());
                        if (entry.find('~') != std::string::npos) continue;
                        if (const auto other = identity_of(it->path()); other && *other == *id) {
                            next = current + joint + entry;
                            break;
                        }
                    }
                }
            }
            current = std::move(next);
        }
        return from_utf8(current);
    }
    // Elsewhere the part that exists has its links followed and the rest is kept.
    std::error_code ec;
    const auto resolved = std::filesystem::weakly_canonical(from_utf8(whole), ec);
    if (ec || resolved.empty()) return from_utf8(whole);
    return from_utf8(generic(resolved));
}

result<void> rename(const std::filesystem::path& from, const std::filesystem::path& to, bool replace) {
    const std::string doing = std::format("renaming {} to {}", generic_utf8(from), generic_utf8(to));
    auto a = where(from, "renaming");
    if (!a) return std::unexpected(a.error());
    auto b = where(to, "renaming");
    if (!b) return std::unexpected(b.error());
    const int rc = kal_fs_rename_flags(dir_of(*a), a->relative.data(), a->relative.size(),
                                       dir_of(*b), b->relative.data(), b->relative.size(),
                                       replace ? 0 : kal::fs::no_replace.bits);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    return {};
}

}  // namespace mcpplibs::os::fs

// --- 0.2.0: appending, durability, long names, measured volumes ----------------

namespace mcpplibs::os::fs {

result<void> append(const std::filesystem::path& file, std::string_view bytes) {
    const std::string doing = std::format("appending to {}", generic_utf8(file));
    auto l = where(file, "appending to");
    if (!l) return std::unexpected(l.error());
    opened_file f;
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::append;
    const int rc = kal_fs_open(dir_of(*l), l->relative.data(), l->relative.size(), flags.bits, &f.f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    // One write: a stream that appends places each write whole at the end.
    const kal_intptr n = kal_stream_write(kal_fs_stream(f.f), bytes.data(), bytes.size());
    if (n < 0 || static_cast<std::size_t>(n) != bytes.size())
        return std::unexpected(from_kal(n < 0 ? static_cast<int>(-n) : kal_err_io, doing));
    return {};
}

result<void> sync(const std::filesystem::path& node) { return sys::sync(node); }

std::string windows_extended(std::string_view in) {
    std::string s(in);
    for (auto& c : s) if (c == '/') c = '\\';
    // Already extended (`\\?\`) or a device name (`\\.\`): nothing is rewritten.
    if (s.starts_with("\\\\?\\") || s.starts_with("\\\\.\\")) return s;
    std::string prefix, rest;
    if (s.starts_with("\\\\")) {
        prefix = "\\\\?\\UNC\\";
        rest = s.substr(2);
    } else if (s.size() >= 3 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' && s[2] == '\\') {
        prefix = "\\\\?\\";
        rest = s;
    } else {
        return std::string(in);   // relative or rootless
    }
    // The prefix turns off `.' and `..', so they are resolved here; `..' never
    // climbs above the drive or the share.
    std::vector<std::string> parts;
    const std::size_t keep = prefix.ends_with("UNC\\") ? 2 : 1;
    std::size_t start = 0;
    while (start <= rest.size()) {
        auto end = rest.find('\\', start);
        if (end == std::string::npos) end = rest.size();
        std::string comp = rest.substr(start, end - start);
        start = end + 1;
        if (comp.empty() || comp == ".") continue;
        if (comp == "..") { if (parts.size() > keep) parts.pop_back(); continue; }
        parts.push_back(std::move(comp));
    }
    std::string out = prefix;
    for (std::size_t i = 0; i < parts.size(); ++i) { if (i) out += '\\'; out += parts[i]; }
    if (parts.size() == 1 && keep == 1) out += '\\';   // `\\?\C:\`, not `\\?\C:`
    return out;
}

std::filesystem::path extended(const std::filesystem::path& name) {
    if (host() != platform::windows || name.empty()) return name;
    std::error_code ec;
    const auto abs = std::filesystem::absolute(name, ec);
    if (ec) return name;
    const auto u8 = abs.generic_u8string();
    const auto spelled = windows_extended(std::string(reinterpret_cast<const char*>(u8.data()), u8.size()));
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(spelled.data()), spelled.size()));
}

std::string volume_type(const std::filesystem::path& directory) { return sys::volume_type(directory); }
owner owner_of(const std::filesystem::path& node) { return sys::owner_of(node); }
bool writable(const std::filesystem::path& node) { return sys::writable(node); }

behaviour measure_now(const std::filesystem::path& directory) {
    behaviour b;
    b.type = sys::volume_type(directory);
    std::error_code ec;
    std::random_device random;
    const auto scratch = directory / std::format(".os-measure-{:08x}{:08x}", random(), random());
    if (!std::filesystem::create_directory(scratch, ec) || ec) return b;
    const auto touch = [&](const std::filesystem::path& p) { return write(p, "x").has_value(); };
    if (touch(scratch / "a") && touch(scratch / "c")) {
        b.measured = true;
        if (const auto r = rename(scratch / "a", scratch / "b", false); !r) {
            b.rename_no_replace = r.error().code != errc::not_supported;
            std::filesystem::rename(scratch / "a", scratch / "b", ec);
        }
        if (const int e = sys::exchange(scratch / "b", scratch / "c"); e != 0)
            b.exchange = !sys::exchange_unsupported(e);
        ec.clear();
        std::filesystem::create_hard_link(scratch / "b", scratch / "d", ec);
        b.hard_links = !ec;
        ec.clear();
        std::filesystem::create_symlink("b", scratch / "e", ec);
        b.symbolic_links = !ec;
        ec.clear();
        if (touch(scratch / "Case")) b.case_sensitive = !std::filesystem::exists(scratch / "cASE", ec);
    }
    std::filesystem::remove_all(scratch, ec);
    return b;
}

behaviour measure(const std::filesystem::path& directory) {
    static std::mutex guard;
    static std::map<std::uint64_t, behaviour> known;
    const auto id = identity_of(directory);
    const std::uint64_t volume = id ? id->volume : 0;
    {
        std::scoped_lock g(guard);
        if (volume != 0)
            if (const auto it = known.find(volume); it != known.end()) return it->second;
    }
    auto b = measure_now(directory);
    if (volume != 0 && b.measured) {
        std::scoped_lock g(guard);
        known.emplace(volume, b);
    }
    return b;
}

result<void> place(const std::filesystem::path& from, const std::filesystem::path& to) {
    const std::string doing = std::format("placing {} as {}", generic_utf8(from), generic_utf8(to));
    auto parent = to.parent_path();
    if (parent.empty()) parent = ".";
    const auto b = measure(parent);
    const auto failed = [&](std::error_code e) {
        return std::unexpected(error{ e == std::errc::file_exists ? errc::exists : errc::io,
                                      std::format("{}: {}", doing, e.message()) });
    };
    if (b.rename_no_replace) {
        const auto r = rename(from, to, false);
        if (r || r.error().code != errc::not_supported) return r;
    }
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(from, ec);
    if (ec) return failed(ec);
    if (status.type() == std::filesystem::file_type::symlink) {
        std::filesystem::copy_symlink(from, to, ec);
        if (ec) return failed(ec);
        std::filesystem::remove(from, ec);
        return {};
    }
    if (status.type() != std::filesystem::file_type::directory && b.hard_links) {
        std::filesystem::create_hard_link(from, to, ec);
        if (ec) return failed(ec);
        std::filesystem::remove(from, ec);
        return {};
    }
    const auto there = std::filesystem::symlink_status(to, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) return failed(ec);
    if (std::filesystem::exists(there)) return failed(std::make_error_code(std::errc::file_exists));
    std::filesystem::rename(from, to, ec);
    if (ec) return failed(ec);
    return {};
}

result<void> exchange(const std::filesystem::path& a, const std::filesystem::path& b) {
    const std::string doing = std::format("exchanging {} and {}", generic_utf8(a), generic_utf8(b));
    auto parent = b.parent_path();
    if (parent.empty()) parent = ".";
    if (measure(parent).exchange) {
        const int e = sys::exchange(a, b);
        if (e == 0) return {};
        if (!sys::exchange_unsupported(e))
            return std::unexpected(error{ errc::io, std::format("{}: {}", doing, std::generic_category().message(e)), e });
    }
    std::random_device random;
    const auto aside = parent / std::format(".os-exchange-{:08x}{:08x}", random(), random());
    std::error_code ec;
    std::filesystem::rename(b, aside, ec);
    if (ec) return std::unexpected(error{ errc::io, std::format("{}: {}", doing, ec.message()) });
    std::filesystem::rename(a, b, ec);
    if (ec) {
        std::error_code back;
        std::filesystem::rename(aside, b, back);
        return std::unexpected(error{ errc::io, std::format("{}: {}", doing, ec.message()) });
    }
    std::filesystem::rename(aside, a, ec);
    if (ec) return std::unexpected(error{ errc::io, std::format("{}: {}", doing, ec.message()) });
    return {};
}

}  // namespace mcpplibs::os::fs
