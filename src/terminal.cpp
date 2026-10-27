module mcpplibs.os.terminal;

import std;
import openkal.types;
import openkal.stream;
import openkal.terminal;
import mcpplibs.os.env;

namespace mcpplibs::os::terminal {

namespace {
kal_stream handle(stream s) {
    switch (s) {
        case stream::in:  return kal_stdin();
        case stream::out: return kal_stdout();
        case stream::err: return kal_stderr();
    }
    return kal_stdout();
}
}  // namespace

bool interactive(stream s) {
    kal_uintptr mode = 0;
    return kal_terminal_get_mode(handle(s), &mode) == kal_ok;
}

std::optional<dimensions> size(stream s) {
    kal_uintptr cols = 0, rows = 0;
    if (kal_terminal_size(handle(s), &cols, &rows) != kal_ok || cols == 0) return std::nullopt;
    return dimensions{ cols, rows };
}

bool sequences(stream s) {
    const kal_stream h = handle(s);
    kal_uintptr mode = 0;
    if (kal_terminal_get_mode(h, &mode) != kal_ok) return false;
    const auto want = kal::terminal::sequences.bits;
    if ((mode & want) == 0) {
        kal_terminal_set_mode(h, mode | want);
        if (kal_terminal_get_mode(h, &mode) != kal_ok) return false;
    }
    return (mode & want) != 0;
}

bool color(stream s) {
    if (const auto v = env::get("NO_COLOR"); v && !v->empty()) return false;
    if (const auto v = env::get("CLICOLOR_FORCE"); v && !v->empty() && *v != "0") return true;
    return interactive(s) && sequences(s);
}

bool foreground() { return interactive(stream::in) && sys::foreground(); }

result<raw_mode> raw_mode::enter(raw_options options) {
    const kal_stream in = kal_stdin();
    kal_uintptr previous = 0;
    if (const int rc = kal_terminal_get_mode(in, &previous); rc != kal_ok)
        return std::unexpected(from_kal(rc, "reading the terminal's mode"));
    const kal_uintptr raw = options.keep_interrupt
        ? kal::terminal::pass_input.bits
        : (kal::terminal::pass_control | kal::terminal::pass_input).bits;
    if (const int rc = kal_terminal_set_mode(in, raw); rc != kal_ok)
        return std::unexpected(from_kal(rc, "setting the terminal's mode"));
    raw_mode r;
    r.previous_ = previous;
    r.active_ = true;
    return r;
}

raw_mode::raw_mode(raw_mode&& o) noexcept
    : previous_(o.previous_), active_(std::exchange(o.active_, false)) {}

raw_mode& raw_mode::operator=(raw_mode&& o) noexcept {
    if (this != &o) {
        if (active_) kal_terminal_set_mode(kal_stdin(), static_cast<kal_uintptr>(previous_));
        previous_ = o.previous_;
        active_ = std::exchange(o.active_, false);
    }
    return *this;
}

raw_mode::~raw_mode() {
    if (active_) kal_terminal_set_mode(kal_stdin(), static_cast<kal_uintptr>(previous_));
}

}  // namespace mcpplibs::os::terminal
