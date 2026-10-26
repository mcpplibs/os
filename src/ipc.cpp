module mcpplibs.os.ipc;

import std;
import openkal.types;
import openkal.fs;
import openkal.stream;
import openkal.net;
import openkal.timeout;
import mcpplibs.os.error;
import mcpplibs.os.fs;
import mcpplibs.os.dirs;

namespace mcpplibs::os::ipc {

namespace {

struct place { kal_dir dir{}; bool owned = false; std::string doing; };

// The directory, opened, for a name to be bound in it.
result<place> directory_of(const std::filesystem::path& d, std::string_view name, std::string_view verb) {
    place p;
    p.doing = std::format("{} {} in {}", verb, name, d.generic_string());
    auto l = fs::locate(d);
    if (!l) return std::unexpected(error{ errc::not_found, p.doing + ": beneath no directory this program was given" });
    p.dir = kal_dir{ static_cast<kal_uintptr>(l->directory) };
    if (l->relative != ".") {
        if (const int rc = kal_fs_open_dir(p.dir, l->relative.data(), l->relative.size(), &p.dir); rc != kal_ok)
            return std::unexpected(from_kal(rc, p.doing));
        p.owned = true;
    }
    return p;
}

void release(place& p) { if (p.owned) kal_fs_close_dir(p.dir); p.owned = false; }

kal_u64 ns(std::chrono::milliseconds t) {
    const auto n = std::chrono::duration_cast<std::chrono::nanoseconds>(t).count();
    return static_cast<kal_u64>(n < 1 ? 1 : n);
}

}  // namespace

result<connection> connection::connect(const std::filesystem::path& directory, std::string_view name) {
    auto p = directory_of(directory, name, "connecting to");
    if (!p) return std::unexpected(p.error());
    kal_net_conn c{};
    const int rc = kal_net_connect_at(p->dir, name.data(), name.size(), &c);
    const std::string doing = p->doing;
    release(*p);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    connection out;
    out.conn_ = static_cast<std::uintptr_t>(c.h);
    return out;
}

connection connection::adopt(std::uintptr_t c) {
    connection out;
    out.conn_ = c;
    return out;
}

connection::connection(connection&& o) noexcept : conn_(std::exchange(o.conn_, 0)) {}
connection& connection::operator=(connection&& o) noexcept {
    if (this != &o) { close(); conn_ = std::exchange(o.conn_, 0); }
    return *this;
}
connection::~connection() { close(); }

result<std::string> connection::read(std::size_t at_most) {
    if (!conn_) return std::string{};
    std::string out(at_most, '\0');
    const kal_intptr n = kal_stream_read(kal_net_stream(kal_net_conn{ conn_ }), out.data(), out.size());
    if (n < 0) {
        if (-n == kal_err_closed) return std::string{};
        return std::unexpected(from_kal(static_cast<int>(-n), "reading a local connection"));
    }
    out.resize(static_cast<std::size_t>(n));
    return out;
}

result<std::optional<std::string>> connection::read_for(std::chrono::milliseconds t, std::size_t at_most) {
    if (!conn_) return std::optional<std::string>{ std::string{} };
    std::string out(at_most, '\0');
    const kal_intptr n = kal_timeout_read(kal_net_stream(kal_net_conn{ conn_ }), out.data(), out.size(), ns(t));
    if (n < 0) {
        if (-n == kal_err_again) return std::optional<std::string>{};
        if (-n == kal_err_closed) return std::optional<std::string>{ std::string{} };
        return std::unexpected(from_kal(static_cast<int>(-n), "reading a local connection"));
    }
    out.resize(static_cast<std::size_t>(n));
    return std::optional<std::string>{ std::move(out) };
}

result<void> connection::write(std::string_view bytes) {
    if (!conn_) return std::unexpected(error{ errc::closed, "writing a local connection" });
    std::lock_guard g(write_lock_);
    const kal_intptr n = kal_stream_write(kal_net_stream(kal_net_conn{ conn_ }), bytes.data(), bytes.size());
    if (n < 0) return std::unexpected(from_kal(static_cast<int>(-n), "writing a local connection"));
    return {};
}

void connection::shutdown_write() {
    if (conn_) kal_net_shutdown(kal_net_conn{ conn_ }, 2 /* KAL_SHUT_WRITE */);
}

void connection::shutdown() {
    if (conn_) kal_net_shutdown(kal_net_conn{ conn_ }, 3 /* KAL_SHUT_BOTH */);
}

void connection::close() {
    if (conn_) { kal_net_close(kal_net_conn{ conn_ }); conn_ = 0; }
}

result<listener> listener::listen(const std::filesystem::path& directory, std::string_view name) {
    auto p = directory_of(directory, name, "listening at");
    if (!p) return std::unexpected(p.error());
    kal_net_listener l{};
    int rc = kal_net_listen_at(p->dir, name.data(), name.size(), &l);
    if (rc == kal_err_exists) {
        // Left behind, or in use: in use answers a connection.
        kal_net_conn probe{};
        if (kal_net_connect_at(p->dir, name.data(), name.size(), &probe) == kal_ok) {
            kal_net_close(probe);
        } else {
            kal_fs_remove(p->dir, name.data(), name.size());
            rc = kal_net_listen_at(p->dir, name.data(), name.size(), &l);
        }
    }
    const std::string doing = p->doing;
    release(*p);
    if (rc != kal_ok) return std::unexpected(from_kal(rc, doing));
    listener out;
    out.listener_ = static_cast<std::uintptr_t>(l.h);
    out.where_ = directory / std::filesystem::path(std::string(name));
    return out;
}

listener::listener(listener&& o) noexcept
    : listener_(std::exchange(o.listener_, 0)), where_(std::move(o.where_)) {}
listener& listener::operator=(listener&& o) noexcept {
    if (this != &o) { close(); listener_ = std::exchange(o.listener_, 0); where_ = std::move(o.where_); }
    return *this;
}
listener::~listener() { close(); }

result<connection> listener::accept() {
    if (!listener_) return std::unexpected(error{ errc::closed, "accepting on a closed listener" });
    kal_net_conn c{};
    if (const int rc = kal_net_accept(kal_net_listener{ listener_ }, &c); rc != kal_ok)
        return std::unexpected(from_kal(rc, "accepting a local connection"));
    connection out;
    out.conn_ = static_cast<std::uintptr_t>(c.h);
    return out;
}

result<std::optional<connection>> listener::accept_for(std::chrono::milliseconds t) {
    if (!listener_) return std::unexpected(error{ errc::closed, "accepting on a closed listener" });
    kal_net_conn c{};
    const int rc = kal_timeout_accept(kal_net_listener{ listener_ }, ns(t), &c);
    if (rc == kal_err_again) return std::optional<connection>{};
    if (rc != kal_ok) return std::unexpected(from_kal(rc, "accepting a local connection"));
    connection out;
    out.conn_ = static_cast<std::uintptr_t>(c.h);
    return std::optional<connection>{ std::move(out) };
}

void listener::close() {
    if (!listener_) return;
    kal_net_close_listener(kal_net_listener{ listener_ });
    listener_ = 0;
    if (auto l = fs::locate(where_)) {
        kal_fs_remove(kal_dir{ static_cast<kal_uintptr>(l->directory) }, l->relative.data(), l->relative.size());
    }
}

result<instance> single_instance(std::string_view name, std::filesystem::path directory) {
    if (directory.empty()) {
        auto r = dirs::runtime();
        if (!r) return std::unexpected(error{ errc::not_found, "no runtime directory" });
        directory = *r;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (auto c = connection::connect(directory, name)) return instance{ std::move(*c) };
        auto l = listener::listen(directory, name);
        if (l) return instance{ std::move(*l) };
        if (l.error().code != errc::exists) return std::unexpected(l.error());
    }
    return std::unexpected(error{ errc::again, std::format("becoming or reaching the instance {}", name) });
}

}  // namespace mcpplibs::os::ipc
