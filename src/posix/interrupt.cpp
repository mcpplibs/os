// The system's ending for a closed reader: SIGPIPE at its default, raised, by
// the kernel's calls (src/posix/kernel.h). openkal ignores the signal in this
// program so that a write to a closed reader is reported; the disposition is
// the process's, so it is restored where the process keeps it.
module;
#include "kernel.h"

module mcpplibs.os.interrupt;

import std;

namespace mcpplibs::os::interrupt::sys {

void end_as_output_closed() {
    mcpplibs::os::kernel::default_signal(mcpplibs::os::kernel::sigpipe);
    mcpplibs::os::kernel::raise_signal(mcpplibs::os::kernel::sigpipe);
    std::_Exit(128 + mcpplibs::os::kernel::sigpipe);
}

}  // namespace mcpplibs::os::interrupt::sys
