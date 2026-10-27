// The system's ending for a closed reader: SIGPIPE at its default, raised.
module;
#include <csignal>
#include <unistd.h>

module mcpplibs.os.interrupt;

namespace mcpplibs::os::interrupt::sys {

void end_as_output_closed() {
    std::signal(SIGPIPE, SIG_DFL);
    std::raise(SIGPIPE);
    ::_exit(128 + SIGPIPE);
}

}  // namespace mcpplibs::os::interrupt::sys
