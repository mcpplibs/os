// mcpplibs.os.ipc and .net: a listener and a connection in one program.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

int main() {
    const auto dir = std::filesystem::temp_directory_path();
    auto caps = os::fs::caps(dir);
    if (caps && caps->local_endpoint) {
        const auto name = std::format("os-ipc-{}", os::process::self()->id);
        auto l = os::ipc::listener::listen(dir, name);
        check(l.has_value(), "a listener is opened at a name");
        if (l) {
            std::jthread server([&] {
                auto c = l->accept();
                if (!c) return;
                auto got = c->read();
                if (got) (void)c->write("re:" + *got);
            });
            auto c = os::ipc::connection::connect(dir, name);
            check(c.has_value(), "a connection is made at the name");
            if (c) {
                check(c->write("ping").has_value(), "bytes are written");
                auto r = c->read();
                check(r && *r == "re:ping", "and the answer is read");
            }
        }
        auto inst = os::ipc::single_instance(name + "-single", dir);
        check(inst && std::holds_alternative<os::ipc::listener>(*inst), "the first instance listens");
        auto second = os::ipc::single_instance(name + "-single", dir);
        check(second && std::holds_alternative<os::ipc::connection>(*second), "a second one connects to it");
    } else {
        std::printf("  (the temporary directory's volume holds no local endpoint)\n");
    }
    {
        auto l = os::net::loopback_listener::listen();
        check(l && l->port() != 0, "a loopback listener is given a port");
        if (l) {
            std::jthread server([&] { auto c = l->accept(); if (c) (void)c->write("hi"); });
            auto c = os::net::connect_loopback(l->port());
            auto r = c ? c->read() : os::result<std::string>{};
            check(r && *r == "hi", "and a loopback connection reads what was written");
        }
    }
    return g_failures;
}
