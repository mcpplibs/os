module;
#include <unistd.h>

module mcpplibs.os.posix;

import std;

namespace mcpplibs::os::posix {

std::uint32_t effective_user() { return static_cast<std::uint32_t>(::geteuid()); }

}  // namespace mcpplibs::os::posix
