// Whether this program is in its terminal's foreground process group.
module;
#include <unistd.h>

module mcpplibs.os.terminal;

namespace mcpplibs::os::terminal::sys {
bool foreground() { return ::tcgetpgrp(0) == ::getpgrp(); }
}  // namespace mcpplibs::os::terminal::sys
