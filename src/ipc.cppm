// mcpplibs.os.ipc --- programs on one machine speaking at a name.
//
// A listener and a connection meet at a name in a directory (openkal 0.17,
// `kal_net_listen_at'), reachable by a program that holds the directory and by
// no other --- unlike an address on the loopback, which every program on the
// machine reaches. Composed here: the runtime directory (`dirs::runtime'),
// removal of a name a listener that ended left behind, and the first of
// several instances becoming the one the others connect to.
export module mcpplibs.os.ipc;

import std;
export import mcpplibs.os.error;

export namespace mcpplibs::os::ipc {

class connection {
public:
    static result<connection> connect(const std::filesystem::path& directory, std::string_view name);
    connection() = default;
    connection(connection&&) noexcept;
    connection& operator=(connection&&) noexcept;
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
    ~connection();
    bool valid() const noexcept { return conn_ != 0; }
    // One chunk; empty at the end.
    result<std::string> read(std::size_t at_most = 65536);
    result<std::optional<std::string>> read_for(std::chrono::milliseconds timeout, std::size_t at_most = 65536);
    result<void> write(std::string_view bytes);
    void shutdown_write();
    // Both directions ended: a thread reading is woken with the end, a thread
    // writing fails. Safe from another thread while one reads or writes; the
    // handle itself is released by `close', once nothing uses it.
    void shutdown();
    void close();
    // A connection openkal made elsewhere (`kal_net_conn', carried as its word),
    // adopted: this object releases it.
    static connection adopt(std::uintptr_t kal_net_conn);
private:
    friend class listener;
    std::uintptr_t conn_ = 0;
    std::mutex     write_lock_;
};

class listener {
public:
    // At `name' in `directory'. A name left behind by a listener that has
    // ended --- nothing answers at it --- is removed and taken.
    static result<listener> listen(const std::filesystem::path& directory, std::string_view name);
    listener() = default;
    listener(listener&&) noexcept;
    listener& operator=(listener&&) noexcept;
    listener(const listener&) = delete;
    listener& operator=(const listener&) = delete;
    // Closed, and the name removed.
    ~listener();
    bool valid() const noexcept { return listener_ != 0; }
    result<connection> accept();
    result<std::optional<connection>> accept_for(std::chrono::milliseconds timeout);
    void close();
private:
    std::uintptr_t        listener_ = 0;
    std::filesystem::path where_;
};

// The first program to ask becomes the listener; a later one is connected to
// it. The name is in `dirs::runtime()' unless a directory is given.
using instance = std::variant<listener, connection>;
result<instance> single_instance(std::string_view name, std::filesystem::path directory = {});

}  // namespace mcpplibs::os::ipc
