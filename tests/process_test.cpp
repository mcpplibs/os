// mcpplibs.os.process: this test starts copies of itself.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

// What starts this program again with an errand.
os::process::spawn_options copy_of_self(std::string errand) {
    const auto again = os::env::self_command();
    os::process::spawn_options o;
    if (again.empty()) return o;
    o.program = again.front();
    o.args.assign(again.begin() + 1, again.end());
    o.args.push_back("--child");
    o.args.push_back(std::move(errand));
    return o;
}

int child(std::string_view errand) {
    if (errand == "say") {
        (void)os::stdio::write_output("said");
        return 0;
    }
    if (errand == "relay") {
        // A program whose own output is a protocol: what it starts writes to
        // its error output instead.
        auto o = copy_of_self("say");
        o.out = os::process::stdio::error;
        o.err = os::process::stdio::inherit;
        auto p = os::process::process::spawn(o);
        if (!p) return 1;
        auto st = p->wait();
        return st && st->success() ? 0 : 2;
    }
    if (errand == "leave") {
        // Leaves a program behind that holds this one's output open.
        auto o = copy_of_self("sleep");
        o.out = os::process::stdio::inherit;
        o.err = os::process::stdio::null;
        auto p = os::process::process::spawn(o);
        if (!p) return 1;
        p->detach();
        return 0;
    }
    if (errand == "echo") {
        auto in = os::stdio::read_exactly(5);
        (void)os::stdio::write_output(in.value_or(""));
        (void)os::stdio::write_error("err");
        return 7;
    }
    if (errand == "env") {
        (void)os::stdio::write_output(os::env::get("OS_TEST_VALUE").value_or("<unset>"));
        return 0;
    }
    if (errand == "cwd") {
        (void)os::stdio::write_output(std::filesystem::current_path().filename().string());
        return 0;
    }
    if (errand == "both") {
        // Written in turn to both streams: a reader of one pipe for both sees
        // them in this order.
        (void)os::stdio::write_output("1");
        (void)os::stdio::write_error("2");
        (void)os::stdio::write_output("3");
        return 0;
    }
    if (errand == "become") {
        // This program becomes another copy of itself that returns 7.
        auto o = copy_of_self("seven");
        o.out = os::process::stdio::inherit;
        o.err = os::process::stdio::inherit;
        auto r = os::process::exec(o);
        (void)os::stdio::write_error(r ? "returned" : r.error().message());
        return 1;
    }
    if (errand == "seven") {
        (void)os::stdio::write_output("became");
        return 7;
    }
    if (errand == "wait-input") {
        // Nothing arrives within the bound; then the input arrives and is read.
        auto none = os::stdio::read_input_for(std::chrono::milliseconds(100));
        if (none && none->has_value()) return 2;
        auto some = os::stdio::read_input_for(std::chrono::seconds(20));
        (void)os::stdio::write_output(some && some->has_value() ? **some : std::string("<none>"));
        return none ? 0 : 3;
    }
    if (errand == "divert") {
        auto d = os::stdio::output_to_error::begin();
        if (!d) return 4;
        (void)os::stdio::write_output("narration");
        (void)d->write_original("document");
        return 0;
    }
    if (errand == "end-closed") {
        os::interrupt::end_as_output_closed();
        return 5;   // where the system has no such ending
    }
    if (errand == "sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    if (errand == "interrupt") {
        (void)os::stdio::write_output(os::interrupt::available() ? "armed\n" : "none\n");
        os::interrupt::wait(std::chrono::seconds(20));
        os::interrupt::end_as_requested();
        return 3;
    }
    return 99;
}

int main(int argc, char** argv) {
    if (argc > 2 && std::string_view(argv[1]) == "--child") return child(argv[2]);
    const auto self = os::env::image();
    check(self.has_value(), "this program's image is named");
    if (!self) return g_failures;
    // What starts this program again: its image, or in an ok image the loader
    // and the image.
    const auto again = os::env::self_command();
    check(!again.empty(), "what starts this program again is known");
    auto copy = [&](std::string errand) {
        os::process::spawn_options o;
        o.program = again.front();
        o.args.assign(again.begin() + 1, again.end());
        o.args.push_back("--child");
        o.args.push_back(std::move(errand));
        return o;
    };

    {
        auto o = copy("echo");
        auto r = os::process::run(o, "hello");
        if (!r) std::printf("  (the start of %s failed: %s)\n", o.program.string().c_str(), r.error().message().c_str());
        check(r && r->out == "hello", "a started program reads its input and writes its output");
        check(r && r->err == "err", "and its error output is collected beside");
        check(r && r->status.code == 7 && !r->status.by_environment, "and its status is the one it returned");
    }
    {
        auto o = copy("env");
        o.env = { { "OS_TEST_VALUE", std::string("set") } };
        auto r = os::process::run(o);
        check(r && r->out == "set", "an environment override reaches the program");
        o.env = { { "OS_TEST_VALUE", std::nullopt } };
        r = os::process::run(o);
        check(r && r->out == "<unset>", "and a removal removes");
    }
    {
        const auto dir = std::filesystem::temp_directory_path() / "mcpplibs-os-cwd-probe";
        std::filesystem::create_directories(dir);
        auto o = copy("cwd");
        o.cwd = dir;
        auto r = os::process::run(o);
        check(r && r->out == "mcpplibs-os-cwd-probe", "a program runs in the directory it is given");
    }
    {
        auto o = copy("sleep");
        o.out = os::process::stdio::null;
        o.err = os::process::stdio::null;
        o.own_unit = true;
        auto p = os::process::process::spawn(o);
        check(p.has_value(), "a program is started in a unit of its own");
        if (p) {
            check(p->id() != 0, "and has an identifier");
            auto d = os::process::describe(p->id());
            check(d && !d->ended, "which another program can ask about");
            auto w = p->wait_for(std::chrono::milliseconds(50));
            check(w && !w->has_value(), "a wait with a bound expires while it runs");
            p->kill();
            auto st = p->wait();
            check(st && !st->success(), "and the unit, ended, does not report success");
        }
    }
    {
        auto r = os::process::run(copy("relay"));
        check(r && r->status.success() && r->out.empty() && r->err == "said",
              "a program started with its output on the error output writes there");
    }
    {
        auto o = copy("leave");
        o.in = os::process::stdio::null;
        o.err = os::process::stdio::null;
        o.own_unit = true;
        auto p = os::process::process::spawn(o);
        check(p.has_value(), "a program that leaves another behind is started in a unit");
        if (p) {
            auto st = p->wait();
            check(st && st->success(), "and ends");
            auto held = p->read_output_for(std::chrono::milliseconds(300));
            check(held && !held->has_value(), "while what it left holds its output open");
            p->end_unit();
            auto end = p->read_output_for(std::chrono::seconds(20));
            check(end && end->has_value() && (*end)->empty(), "and ending the rest of its unit closes it");
            check(p->reads_bounded(), "and every bounded read here was bounded");
        }
    }
    if (os::interrupt::available()) {
        auto o = copy("interrupt");
        auto p = os::process::process::spawn(o);
        if (p) {
            auto first = p->read_output();
            check(first && first->starts_with("armed"), "a started program takes requests to end over");
            p->terminate();
            auto st = p->wait();
            if constexpr (os::is_windows) {
                check(st.has_value(), "a program asked to end is awaited");
            } else {
                check(st && st->by_environment, "and ends as the request would have ended it");
            }
        }
    }
    {
        auto o = copy("both");
        o.out = os::process::stdio::pipe;
        o.err = os::process::stdio::output;
        auto p = os::process::process::spawn(o);
        std::string all;
        if (p) {
            for (;;) { auto c = p->read_output(); if (!c || c->empty()) break; all += *c; }
            (void)p->wait();
        }
        check(all == "123", "one pipe carries a program's output and error output in the order written");
    }
    {
        auto o = copy("become");
        auto r = os::process::run(o);
        check(r && r->out == "became" && r->status.code == 7,
              "a program that becomes another ends with that program's status and output");
    }
    if (!os::is_image) {
        auto o = copy("wait-input");
        o.in = os::process::stdio::pipe;
        o.out = os::process::stdio::pipe;
        auto p = os::process::process::spawn(o);
        if (p) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            (void)p->write("late");
            p->close_input();
            std::string out;
            for (;;) { auto c = p->read_output(); if (!c || c->empty()) break; out += *c; }
            auto st = p->wait();
            check(st && st->code == 0 && out == "late", "a bounded read of input expires, then reads what arrives");
        }
        auto d = os::process::run(copy("divert"));
        check(d && d->out == "document" && d->err == "narration",
              "output sent to the error stream for a while, and the document to the output as it was");
    }
    {
        // The command interpreter given a command with quotation marks, joined
        // as it is; refused where a program receives a vector.
        os::process::spawn_options o;
        o.one_string = true;
        if (os::host() == os::platform::windows) {
            o.program = "cmd.exe";
            o.args = { "/d", "/s", "/c", "\"echo \"a b\" && echo c\"" };
            auto r = os::process::run(o);
            check(r && r->out.find("\"a b\"") != std::string::npos && r->out.find('c') != std::string::npos,
                  "the command interpreter runs a command with quotation marks as written");
        } else {
            o.program = again.front();
            auto r = os::process::run(o);
            check(!r && r.error().code == os::errc::not_supported,
                  "a joined command line is refused where a program receives a vector");
        }
    }
    if (!os::is_image) {
        auto r = os::process::run(copy("end-closed"));
        if (os::host() == os::platform::windows)
            check(r && r->status.code == 5, "Windows has no ending for a closed reader, and the program goes on");
        else
            check(r && r->status.by_environment && r->status.shell_code() == 141,
                  "a program ends as a closed reader ends it: 141 to a shell");
    }
    {
        auto me = os::process::self();
        check(me && me->id != 0, "this program has an identifier");
        check(me && os::process::alive(*me), "and is alive");
    }
    // The program the system starts: this one, or in an ok image its loader.
    const auto started = os::is_image ? os::env::loader() : self;
    if (started) {
        const auto whole = os::fs::absolute(*started);
        check(os::env::find_executable(whole.filename().string(), whole.parent_path().string()).has_value(),
              "a program is found by name in a list of directories");
    }
    {
        // npm's directory holds `npm.cmd' and an extension-less `npm', the
        // POSIX shell script: on Windows the name finds the first.
        const auto dir = std::filesystem::temp_directory_path() / std::format("mcpplibs-os-find-{}", os::process::self()->id);
        std::filesystem::create_directories(dir);
        (void)os::fs::write(dir / "tool", "#!/bin/sh\n");
        (void)os::fs::write(dir / "tool.cmd", "@echo off\n");
        const auto found = os::env::find_executable("tool", os::fs::absolute(dir).string());
        const bool windows = os::host() == os::platform::windows;
        check(found && found->filename() == (windows ? "tool.cmd" : "tool"),
              "a bare name finds what the system starts: on Windows by an extension, never the bare file");
        const auto named = os::env::find_executable("tool.cmd", os::fs::absolute(dir).string());
        check(named && named->filename() == "tool.cmd", "and a name with its extension is taken as written");
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    return g_failures;
}
