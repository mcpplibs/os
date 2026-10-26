// mcpplibs.os.process: this test starts copies of itself.
#include "check.hpp"
import std;
import mcpplibs.os;

namespace os = mcpplibs::os;

int child(std::string_view errand) {
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

    {
        os::process::spawn_options o;
        o.program = *self;
        o.args = { "--child", "echo" };
        auto r = os::process::run(o, "hello");
        check(r && r->out == "hello", "a started program reads its input and writes its output");
        check(r && r->err == "err", "and its error output is collected beside");
        check(r && r->status.code == 7 && !r->status.by_environment, "and its status is the one it returned");
    }
    {
        os::process::spawn_options o;
        o.program = *self;
        o.args = { "--child", "env" };
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
        os::process::spawn_options o;
        o.program = *self;
        o.args = { "--child", "cwd" };
        o.cwd = dir;
        auto r = os::process::run(o);
        check(r && r->out == "mcpplibs-os-cwd-probe", "a program runs in the directory it is given");
    }
    {
        os::process::spawn_options o;
        o.program = *self;
        o.args = { "--child", "sleep" };
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
    if (os::interrupt::available()) {
        os::process::spawn_options o;
        o.program = *self;
        o.args = { "--child", "interrupt" };
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
        auto me = os::process::self();
        check(me && me->id != 0, "this program has an identifier");
        check(me && os::process::alive(*me), "and is alive");
    }
    check(os::env::find_executable(self->filename().string(), self->parent_path().string()).has_value(),
          "a program is found by name in a list of directories");
    return g_failures;
}
