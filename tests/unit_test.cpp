// Declared units and a retained observer over the existing directory IPC.
#include "check.hpp"
import std;
import mcpplibs.os.process;
import mcpplibs.os.ipc;

namespace process = mcpplibs::os::process;
namespace ipc = mcpplibs::os::ipc;
using clock_type = std::chrono::steady_clock;
using namespace std::chrono_literals;

static std::optional<std::string> line(ipc::connection& c, std::chrono::milliseconds bound) {
    const auto until = clock_type::now() + bound;
    std::string out;
    while (out.size() <= 1024) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - clock_type::now());
        if (left <= 0ms) return {};
        auto read = c.read_for(left, 1024);
        if (!read || !*read || (**read).empty()) return {};
        out += **read;
        if (const auto end = out.find('\n'); end != std::string::npos) {
            if (end + 1 != out.size()) return {};
            out.resize(end); return out;
        }
    }
    return {};
}

int main(int argc, char** argv) {
    const auto self = std::filesystem::absolute(argv[0]);
    if (argc > 2 && std::string_view(argv[1]) == "writer") {
        std::ofstream out(argv[2]); out << "declared-unit writer\n"; return out ? 0 : 40;
    }
    if (argc > 5 && std::string_view(argv[1]) == "helper") {
        auto unit = process::owned_unit::create();
        if (!unit) return 30;
        const std::filesystem::path output(argv[4]);
        if (std::string_view(argv[5]) != "none") {
            auto c = ipc::connection::connect(argv[2],argv[3]);
            if (!c || !c->write(unit->reference() + "\n")) return 31;
            const auto ack = line(*c, std::string_view(argv[5]) == "timeout" ? 50ms : 2s);
            if (!ack || *ack != "ACK") return 31;
        }
        {
            auto writer = process::process::spawn({.program=self, .args={"writer",output.string()},
                .out=process::stdio::null, .err=process::stdio::null, .declared_unit=&*unit});
            if (!writer) return 32;
            const auto status = writer->wait();
            if (!status || !status->success()) return 33;
        }
        const auto limit = clock_type::now() + 2s;
        for (;;) {
            auto retired = unit->retire();
            if (!retired) return 34;
            if (*retired == process::unit_state::empty) break;
            if (clock_type::now() >= limit) return 35;
            std::this_thread::sleep_for(1ms);
        }
        if (auto again = unit->retire(); !again || *again != process::unit_state::empty) return 36;
        auto forbidden = process::process::spawn({.program=self, .args={"writer",output.string()+".forbidden"},
            .declared_unit=&*unit});
        if (forbidden || forbidden.error().code != mcpplibs::os::errc::closed) return 37;
        return 0;
    }

    const auto identity = process::self();
    if (!identity) return 1;
    const auto root = std::filesystem::temp_directory_path() / std::format("os-unit-{}",identity->id);
    std::filesystem::create_directories(root);
    struct Remove { std::filesystem::path path; ~Remove() { std::error_code ec; std::filesystem::remove_all(path,ec); } } remove{root};
    auto attempt = [&](std::string_view mode, int repeat) {
        const auto name = std::format("unit-{}-{}",mode,repeat);
        const auto output = root / (name + ".object");
        std::optional<ipc::listener> listener;
        std::optional<process::unit_observer> observer;
        bool server_ok = true;
        std::jthread server;
        const auto started = clock_type::now();
        if (mode != "none") {
            auto listening = ipc::listener::listen(root,name);
            if (!listening) { check(false,"a unit registration listener is opened"); return; }
            listener.emplace(std::move(*listening));
            server = std::jthread([&] {
                auto accepted = listener->accept_for(2s);
                if (!accepted || !*accepted) { server_ok = false; return; }
                auto reference = line(**accepted,2s);
                if (!reference) { server_ok = false; return; }
                auto retained = process::unit_observer::open(*reference);
                if (!retained) { server_ok = false; return; }
                observer.emplace(std::move(*retained));
                if (mode == "ack") server_ok = bool((**accepted).write("ACK\n"));
                if (mode == "timeout") std::this_thread::sleep_for(100ms);
            });
        }
        int exit = -1;
        {
            auto helper = process::process::spawn({.program=self,
                .args={"helper",root.string(),name,output.string(),std::string(mode)},
                .in=process::stdio::null, .out=process::stdio::null, .err=process::stdio::null});
            if (helper) { auto status = helper->wait(); if (status) exit=status->shell_code(); }
        }
        if (server.joinable()) server.join();
        check(server_ok,"the original unit is retained before acknowledgement");
        const bool rejected = mode == "closed" || mode == "timeout";
        check(exit == (rejected ? 31 : 0),"a compiler starts only after successful registration");
        check(std::filesystem::exists(output) != rejected,"a refused acknowledgement produces no output");
        check(!std::filesystem::exists(output.string()+".forbidden"),"retirement prevents further admission");
        if (observer) {
            auto state = observer->observe();
            check(state && *state == process::unit_state::empty,"the original unit is empty after helper completion");
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now()-started).count();
        std::printf("{\"unit_start_mode\":\"%.*s\",\"elapsed_ns\":%lld}\n",int(mode.size()),mode.data(),static_cast<long long>(elapsed));
    };
    for (int repeat = 0; repeat < 8; ++repeat) { attempt("none",repeat); attempt("ack",repeat); }
    attempt("closed",0); attempt("timeout",0);
    for (int repeat = 0; repeat < 8; ++repeat) {
        const auto name=std::format("idle-{}",repeat);
        const auto started=clock_type::now();
        auto listener=ipc::listener::listen(root,name);
        check(bool(listener),"a no-op listener opens");
        if (!listener) continue;
        std::jthread server([&] { auto peer=listener->accept_for(2s); });
        { auto peer=ipc::connection::connect(root,name); check(bool(peer),"closure wakes the listener without a polling delay"); }
        server.join(); listener->close();
        const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now()-started).count();
        std::printf("{\"unit_listener_noop_ns\":%lld}\n",static_cast<long long>(elapsed));
    }
    return g_failures;
}
