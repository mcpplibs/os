// A console has no foreground group to be outside of.
module mcpplibs.os.terminal;

namespace mcpplibs::os::terminal::sys {
bool foreground() { return true; }
}  // namespace mcpplibs::os::terminal::sys
