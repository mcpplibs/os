// mcpplibs.os.ipc and .net: a listener and a connection in one program.
import std;
import mcpplibs.os;
import mcpplibs.mtest;

namespace os = mcpplibs::os;
using namespace mcpplibs::mtest;

UTest ipc_tests{"ipc", [] {
    const auto dir = std::filesystem::temp_directory_path();
    auto caps = os::fs::caps(dir);
    if (caps && caps->local_endpoint) {
        const auto name = std::format("os-ipc-{}", os::process::self()->id);
        auto l = os::ipc::listener::listen(dir, name);
        expect(l.has_value()) << "a listener is opened at a name";
        if (l) {
            std::jthread server([&] {
                auto c = l->accept();
                if (!c) return;
                auto got = c->read();
                if (got) (void)c->write("re:" + *got);
            });
            auto c = os::ipc::connection::connect(dir, name);
            expect(c.has_value()) << "a connection is made at the name";
            if (c) {
                expect(c->write("ping").has_value()) << "bytes are written";
                auto r = c->read();
                expect(r && *r == "re:ping") << "and the answer is read";
            }
        }
        auto inst = os::ipc::single_instance(name + "-single", dir);
        expect(inst && std::holds_alternative<os::ipc::listener>(*inst)) << "the first instance listens";
        auto second = os::ipc::single_instance(name + "-single", dir);
        expect(second && std::holds_alternative<os::ipc::connection>(*second)) << "a second one connects to it";
    } else {
        std::printf("  (the temporary directory's volume holds no local endpoint)\n");
    }
    {
        auto l = os::net::loopback_listener::listen();
        expect(l && l->port() != 0) << "a loopback listener is given a port";
        if (l) {
            std::jthread server([&] { auto c = l->accept(); if (c) (void)c->write("hi"); });
            auto c = os::net::connect_loopback(l->port());
            auto r = c ? c->read() : os::result<std::string>{};
            expect(r && *r == "hi") << "and a loopback connection reads what was written";
        }
    }
    {
        // A reader woken by another thread's shutdown, while its peer writes nothing; and both
        // directions at once, a write while a read waits.
        auto l = os::net::loopback_listener::listen();
        if (l) {
            std::optional<os::ipc::connection> accepted;
            std::jthread server([&] { if (auto c = l->accept()) accepted.emplace(std::move(*c)); });
            auto c = os::net::connect_loopback(l->port());
            server.join();
            expect(c && accepted) << "a connection is accepted";
            if (c && accepted) {
                std::atomic<bool> woken{ false };
                std::jthread reader([&] { auto r = c->read(); woken = r && r->empty(); });
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                expect(c->write("while a read waits").has_value()) << "a write proceeds while a read on the same connection waits";
                auto echoed = accepted->read();
                expect(echoed && *echoed == "while a read waits") << "and arrives";
                c->shutdown();
                reader.join();
                expect(woken.load()) << "a shutdown from another thread wakes the reader with the end";
            }
        }
    }
}};
