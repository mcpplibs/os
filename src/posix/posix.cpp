module;
#include <sys/stat.h>
#include <unistd.h>

module mcpplibs.os.posix;

import std;

namespace mcpplibs::os::posix {

std::uint32_t effective_user() { return static_cast<std::uint32_t>(::geteuid()); }

void set_creation_mask(std::uint32_t mask) { ::umask(static_cast<mode_t>(mask)); }

}  // namespace mcpplibs::os::posix
