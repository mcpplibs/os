// mcpplibs.os.terminal --- whether a person is there, how wide the display is,
// whether colour will be obeyed, and keystrokes read one at a time.
export module mcpplibs.os.terminal;

import std;
export import mcpplibs.os.error;

export namespace mcpplibs::os::terminal {

enum class stream { in, out, err };

// Whether the stream is a terminal.
bool interactive(stream s);

// The display, in character cells; nothing where it is not known.
struct dimensions { std::size_t columns = 0, rows = 0; };
std::optional<dimensions> size(stream s = stream::out);

// Whether the terminal obeys ECMA-48 control sequences written to it (openkal
// 0.17, KAL_TERM_SEQUENCES), asking for it where the system must be asked.
bool sequences(stream s);

// Whether to write colour: NO_COLOR says never, CLICOLOR_FORCE always, and
// otherwise colour is written to a terminal that obeys the sequences.
bool color(stream s);

// Whether this program may change the terminal's mode: its input is a terminal
// and, on POSIX, this program is in the terminal's foreground --- a background
// job that changed it would be stopped (SIGTTOU) (0.2.0).
bool foreground();

// Keystrokes as they are typed --- no line assembly, no echo, none kept by the
// system, none altered --- until destroyed, when the previous mode returns.
// `keep_interrupt' leaves the system the keystroke that interrupts the program
// (Ctrl-C), so a program that reads keys still ends as a person asks (0.2.0).
struct raw_options { bool keep_interrupt = false; };
class raw_mode {
public:
    static result<raw_mode> enter(raw_options options = {});
    raw_mode() = default;
    raw_mode(raw_mode&&) noexcept;
    raw_mode& operator=(raw_mode&&) noexcept;
    raw_mode(const raw_mode&) = delete;
    raw_mode& operator=(const raw_mode&) = delete;
    ~raw_mode();
private:
    std::uintptr_t previous_ = 0;
    bool active_ = false;
};

}  // namespace mcpplibs::os::terminal

namespace mcpplibs::os::terminal::sys {
bool foreground();
}  // namespace mcpplibs::os::terminal::sys
