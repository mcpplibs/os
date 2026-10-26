// mcpplibs.os.machine in an ok image: see src/openkal/openkal.cpp.
module mcpplibs.os.machine;

import std;
import mcpplibs.os.platform;

namespace mcpplibs::os::machine {

std::string arch() { return std::string(name(ARCH)); }

std::optional<std::uint64_t> memory() { return std::nullopt; }

}  // namespace mcpplibs::os::machine
