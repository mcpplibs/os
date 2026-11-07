// mcpplibs.os.posix on Linux and macOS: the process's user and creation mask,
// asked of the kernel (src/posix/kernel.h) and not of the C library, which in
// the hosted arrangement sits on openkal and knows neither (0.4.0).
module;
#include "kernel.h"

module mcpplibs.os.posix;

import std;

namespace mcpplibs::os::posix {

std::uint32_t effective_user() { return kernel::effective_user(); }

void set_creation_mask(std::uint32_t mask) { kernel::creation_mask(mask); }

}  // namespace mcpplibs::os::posix
