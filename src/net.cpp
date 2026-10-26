module mcpplibs.os.net;

import std;
import openkal.types;
import openkal.net;
import mcpplibs.os.error;
import mcpplibs.os.ipc;

namespace mcpplibs::os::net {

namespace {
kal_endpoint loopback(std::uint16_t port) {
    kal_endpoint e{};
    e.addr[0] = 127; e.addr[1] = 0; e.addr[2] = 0; e.addr[3] = 1;
    e.addr_len = 4;
    e.port = port;
    return e;
}
}  // namespace

result<loopback_listener> loopback_listener::listen(std::uint16_t port) {
    const kal_endpoint at = loopback(port);
    kal_net_listener l{};
    if (const int rc = kal_net_listen(&at, &l); rc != kal_ok)
        return std::unexpected(from_kal(rc, std::format("listening on 127.0.0.1:{}", port)));
    kal_endpoint given{};
    kal_net_listener_local(l, &given);
    loopback_listener out;
    out.listener_ = static_cast<std::uintptr_t>(l.h);
    out.port_ = static_cast<std::uint16_t>(given.port);
    return out;
}

loopback_listener::loopback_listener(loopback_listener&& o) noexcept
    : listener_(std::exchange(o.listener_, 0)), port_(o.port_) {}
loopback_listener& loopback_listener::operator=(loopback_listener&& o) noexcept {
    if (this != &o) { close(); listener_ = std::exchange(o.listener_, 0); port_ = o.port_; }
    return *this;
}
loopback_listener::~loopback_listener() { close(); }

result<ipc::connection> loopback_listener::accept() {
    if (!listener_) return std::unexpected(error{ errc::closed, "accepting on a closed listener" });
    kal_net_conn c{};
    if (const int rc = kal_net_accept(kal_net_listener{ listener_ }, &c); rc != kal_ok)
        return std::unexpected(from_kal(rc, "accepting a loopback connection"));
    return ipc::connection::adopt(static_cast<std::uintptr_t>(c.h));
}

void loopback_listener::close() {
    if (listener_) { kal_net_close_listener(kal_net_listener{ listener_ }); listener_ = 0; }
}

result<ipc::connection> connect_loopback(std::uint16_t port) {
    const kal_endpoint to = loopback(port);
    kal_net_conn c{};
    if (const int rc = kal_net_connect(&to, &c); rc != kal_ok)
        return std::unexpected(from_kal(rc, std::format("connecting to 127.0.0.1:{}", port)));
    return ipc::connection::adopt(static_cast<std::uintptr_t>(c.h));
}

}  // namespace mcpplibs::os::net
