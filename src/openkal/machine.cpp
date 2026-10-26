// mcpplibs.os.machine in an ok image: see src/openkal/openkal.cpp.
module mcpplibs.os.machine;

import std;
import mcpplibs.os.platform;
import openkal.types;
import openkal.task;

namespace mcpplibs::os::machine {

std::string arch() { return std::string(name(ARCH)); }

std::optional<std::uint64_t> memory() { return std::nullopt; }

std::optional<std::uint64_t> memory_available() { return std::nullopt; }

// The parallelism the environment offers, which is all an image is told.
processor_count processors() {
    processor_count c;
    const auto n = kal_task_parallelism();
    c.logical = c.physical = n > 0 ? static_cast<std::uint32_t>(n) : 1u;
    return c;
}

}  // namespace mcpplibs::os::machine
