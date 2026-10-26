// mcpplibs.os.net --- a connection on this machine's loopback, for a peer that
// speaks TCP (an editor's language client, a debugger). A program that chooses
// both ends uses `ipc', which only the holders of a directory reach.
export module mcpplibs.os.net;

import std;
export import mcpplibs.os.error;
export import mcpplibs.os.ipc;

export namespace mcpplibs::os::net {

// A listener on 127.0.0.1, at the port given or, for zero, one the system
// chooses; and a connection to such a port. Both read and write as
// `ipc::connection' does.
class loopback_listener {
public:
    static result<loopback_listener> listen(std::uint16_t port = 0);
    loopback_listener() = default;
    loopback_listener(loopback_listener&&) noexcept;
    loopback_listener& operator=(loopback_listener&&) noexcept;
    loopback_listener(const loopback_listener&) = delete;
    loopback_listener& operator=(const loopback_listener&) = delete;
    ~loopback_listener();
    std::uint16_t port() const noexcept { return port_; }
    result<ipc::connection> accept();
    void close();
private:
    std::uintptr_t listener_ = 0;
    std::uint16_t  port_ = 0;
};

result<ipc::connection> connect_loopback(std::uint16_t port);

}  // namespace mcpplibs::os::net
