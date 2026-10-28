// mcpplibs.os.machine on Emscripten: a wasm module is told nothing of the
// machine beneath its host but the number of processors, as Emscripten's C
// library reports it: in a `-pthread' link the host's (`navigator.
// hardwareConcurrency' in a page, the operating system's count under node),
// and one without, where the module has one context to run in.
module;
#include <emscripten/threading.h>

module mcpplibs.os.machine;

import std;
import mcpplibs.os.platform;

namespace mcpplibs::os::machine {

// The architecture the module was built for, which is the one it runs upon:
// the host translates it, and says nothing of its own.
std::string arch() { return std::string(name(ARCH)); }

// Not known: the module's heap limit is not the machine's memory, and an
// answer guessed here would be wrong somewhere.
std::optional<std::uint64_t> memory() { return std::nullopt; }

std::optional<std::uint64_t> memory_available() { return std::nullopt; }

processor_count processors() {
    processor_count c;
    const int n = ::emscripten_num_logical_cores();
    c.logical = c.physical = n > 0 ? static_cast<std::uint32_t>(n) : 1u;
    return c;
}

}  // namespace mcpplibs::os::machine
