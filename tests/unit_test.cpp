// Declared units and a retained observer over the existing directory IPC.
import std;
import mcpplibs.os.process;
import mcpplibs.os.ipc;
import mcpplibs.os.env;
import mcpplibs.mtest;

namespace process = mcpplibs::os::process;
namespace ipc = mcpplibs::os::ipc;
using clock_type = std::chrono::steady_clock;
using namespace std::chrono_literals;
using namespace mcpplibs::mtest;

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

// This program again as the child `as', given `args'.
static process::spawn_options copy_of_self(std::string_view as, const std::vector<std::string>& args) {
    const auto command = child_command(as, args);
    process::spawn_options options;
    options.program = std::filesystem::u8path(command.front());
    options.args.assign(command.begin() + 1, command.end());
    return options;
}

// The copy a helper starts in its declared unit: it writes its output and ends.
child writer_child{"writer", [](std::vector<std::string> args) {
    if (args.size() != 1) return 40;
    std::ofstream out(args[0]); out << "declared-unit writer\n"; return out ? 0 : 40;
}};

// The copy that registers a declared unit, starts a writer in it and retires it:
// the directory and name of the registration, the output, the mode, and the
// program it continues.
child helper_child{"helper", [](std::vector<std::string> args) -> int {
        if (args.size() != 6) return 30;
        auto unit = process::owned_unit::create();
        if (!unit) return 30;
        const std::filesystem::path output(args[2]);
        const process::program continuation{std::stoull(args[4]),std::stoull(args[5])};
        if (args[3] != "none") {
            auto c = ipc::connection::connect(args[0],args[1]);
            if (!c || !c->write(unit->reference() + "\n")) return 31;
            const auto ack = line(*c, args[3] == "timeout" ? 50ms : 2s);
            if (!ack || *ack != "ACK") return 31;
        }
        {
            auto options = copy_of_self("writer", {output.string()});
            options.out = process::stdio::null;
            options.err = process::stdio::null;
            options.declared_unit = &*unit;
            auto writer = process::process::spawn(options);
            if (!writer) return 32;
            const auto status = writer->wait();
            if (!status || !status->success()) return 33;
        }
        auto wrong=continuation;++wrong.started_ns;
        auto refused=unit->retire(wrong);
        if(!refused||*refused!=process::unit_state::unknown)return 38;
        const auto limit = clock_type::now() + 2s;
        for (;;) {
            auto retired = unit->retire(continuation);
            if (!retired) return 34;
            if (*retired == process::unit_state::empty) break;
            if (clock_type::now() >= limit) return 35;
            std::this_thread::sleep_for(1ms);
        }
        if (auto again = unit->retire(continuation); !again || *again != process::unit_state::empty) return 36;
        auto forbidden_options = copy_of_self("writer", {output.string()+".forbidden"});
        forbidden_options.declared_unit = &*unit;
        auto forbidden = process::process::spawn(forbidden_options);
        if (forbidden || forbidden.error().code != mcpplibs::os::errc::closed) return 37;
        return 0;
}};

UTest unit_tests{"unit", [] {
    const auto identity = process::self();
    expect(fatal(identity.has_value())) << "this program names itself";
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
            if (!listening) { expect(false) << "a unit registration listener is opened"; return; }
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
            auto options = copy_of_self("helper", {root.string(), name, output.string(), std::string(mode),
                std::to_string(identity->id), std::to_string(identity->started_ns)});
            options.in = process::stdio::null;
            options.out = process::stdio::null;
            options.err = process::stdio::null;
            auto helper = process::process::spawn(options);
            if (helper) { auto status = helper->wait(); if (status) exit=status->shell_code(); }
        }
        if (server.joinable()) server.join();
        expect(server_ok) << "the original unit is retained before acknowledgement";
        const bool rejected = mode == "closed" || mode == "timeout";
        expect(exit == (rejected ? 31 : 0)) << "a compiler starts only after successful registration";
        expect(std::filesystem::exists(output) != rejected) << "a refused acknowledgement produces no output";
        expect(!std::filesystem::exists(output.string()+".forbidden")) << "retirement prevents further admission";
        if (observer) {
            auto state = observer->observe();
            expect(state && *state == process::unit_state::empty) << "the original unit is empty after helper completion";
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
        expect(bool(listener)) << "a no-op listener opens";
        if (!listener) continue;
        std::jthread server([&] { auto peer=listener->accept_for(2s); });
        { auto peer=ipc::connection::connect(root,name); expect(bool(peer)) << "closure wakes the listener without a polling delay"; }
        server.join(); listener->close();
        const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now()-started).count();
        std::printf("{\"unit_listener_noop_ns\":%lld}\n",static_cast<long long>(elapsed));
    }
}};
