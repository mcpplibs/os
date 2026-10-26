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

std::string generic(std::string s) {
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
        std::format("{} {}: beneath no directory this program was given", doing, p.generic_string()) });
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

std::filesystem::path absolute(const std::filesystem::path& name) {
    const std::string s = name.generic_string();
    const bool windows = host() == platform::windows;
    const bool already = windows
        ? (s.size() >= 3 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' && separator(s[2]))
              || (s.size() >= 2 && separator(s[0]) && separator(s[1]))
        : !s.empty() && s[0] == '/';
    if (already) return name;
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (ec) return name;
    return std::filesystem::path(generic(cwd.generic_string()) + "/" + s);
}

bool names_a_place(std::string_view name) {
    if (host() == platform::windows)
        return name.find_first_of("/\\") != std::string_view::npos
            || (name.size() >= 2 && std::isalpha(static_cast<unsigned char>(name[0])) && name[1] == ':');
    return name.find('/') != std::string_view::npos;
}

std::optional<located> locate(const std::filesystem::path& name) {
    const auto whole = mcpplibs::os::fs::absolute(name);
    const std::string path = generic(whole.lexically_normal().generic_string());
    const bool fold = conventions().case_insensitive_names;
    std::optional<located> best;
    std::size_t best_len = 0;
    for (const auto& s : table()) {
        if (!s.dir.h || s.name.empty()) continue;
        const std::string prefix = generic(s.name);
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

result<std::string> read(const std::filesystem::path& file) {
    auto l = where(file, "reading");
    if (!l) return std::unexpected(l.error());
    opened_file f;
    const int rc = kal_fs_open(dir_of(*l), l->relative.data(), l->relative.size(),
                               kal::fs::open::read.bits, &f.f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("reading {}", file.generic_string())));
    std::string out;
    char buf[65536];
    for (;;) {
        const kal_intptr n = kal_stream_read(kal_fs_stream(f.f), buf, sizeof buf);
        if (n < 0) return std::unexpected(from_kal(static_cast<int>(-n), std::format("reading {}", file.generic_string())));
        if (n == 0) break;
        out.append(buf, static_cast<std::size_t>(n));
    }
    return out;
}

result<void> write(const std::filesystem::path& file, std::string_view bytes) {
    auto l = where(file, "writing");
    if (!l) return std::unexpected(l.error());
    opened_file f;
    const auto flags = kal::fs::open::write | kal::fs::open::create | kal::fs::open::truncate;
    const int rc = kal_fs_open(dir_of(*l), l->relative.data(), l->relative.size(), flags.bits, &f.f);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("writing {}", file.generic_string())));
    return write_all(f.f, bytes, std::format("writing {}", file.generic_string()));
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
    const std::string doing = std::format("writing {}", file.generic_string());
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
    const std::string doing = std::format("publishing {}", file.generic_string());
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
        if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("asking about {}", directory.generic_string())));
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
    return c;
}

result<lock> lock::acquire(const std::filesystem::path& file, kind k, bool wait) {
    const std::string doing = std::format("locking {}", file.generic_string());
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
    const std::string doing = std::format("linking {} to {}", link.generic_string(), target.generic_string());
    auto l = where(link, "linking");
    if (!l) return std::unexpected(l.error());
    const std::string t = target.generic_string();
    const int rc = kal_fs_link_create(dir_of(*l), l->relative.data(), l->relative.size(), t.data(), t.size(),
                                      directory ? kal::fs::link_directory : 0);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    return {};
}

result<std::string> read_link(const std::filesystem::path& link) {
    const std::string doing = std::format("reading the link {}", link.generic_string());
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
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("marking {}", file.generic_string())));
    return {};
}

namespace {
result<kal_node_info> info(const std::filesystem::path& node, kal_u32 wanted, std::string_view doing) {
    auto l = where(node, doing);
    if (!l) return std::unexpected(l.error());
    kal_node_info i{};
    i.self_size = sizeof i;
    const int rc = kal_fs_info(dir_of(*l), l->relative.data(), l->relative.size(), 0, wanted, &i);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, std::format("{} {}", doing, node.generic_string())));
    if (i.kind == kal_node_absent) return std::unexpected(error{ errc::not_found, std::format("{} {}", doing, node.generic_string()) });
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
        return std::unexpected(error{ errc::not_supported, std::format("the identity of {}", node.generic_string()) });
    return identity{ i->identity[0], i->identity[1] };
}

result<void> rename(const std::filesystem::path& from, const std::filesystem::path& to, bool replace) {
    const std::string doing = std::format("renaming {} to {}", from.generic_string(), to.generic_string());
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
