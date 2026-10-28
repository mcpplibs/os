// A host's terminal has no process groups to be outside of: a module is in
// its foreground whenever it runs.
module mcpplibs.os.terminal;

namespace mcpplibs::os::terminal::sys {
bool foreground() { return true; }
}  // namespace mcpplibs::os::terminal::sys
