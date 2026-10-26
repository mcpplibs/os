// mcpplibs.os.machine --- the machine beneath the system, which is not a
// question of the kernel layer (part 2 §7.2, G7 is not in openkal).
//
// Each answer comes from the system's own service, in an implementation unit
// per system (src/<os>/machine.cpp); an image answers from none of them.
export module mcpplibs.os.machine;

import std;

export namespace mcpplibs::os::machine {

// The processor architecture the machine has, which is not always the one this
// program was built for: a program translated for another (Rosetta, WoW64,
// an emulator registered with the kernel) is told the machine's. `x86_64',
// `aarch64', `riscv64', or the system's own word.
std::string arch();

// The physical memory, in bytes.
std::optional<std::uint64_t> memory();

// The memory the system could give a program now without paging, in bytes:
// MemAvailable on Linux, free and inactive pages on macOS, the available
// physical memory on Windows (0.2.0).
std::optional<std::uint64_t> memory_available();

// The processors: those online, the cores beneath them, and whether the cores
// differ in speed (performance and efficiency cores), when a count of cores
// overstates the work they do together (0.2.0).
struct processor_count {
    std::uint32_t logical = 1;
    std::uint32_t physical = 1;
    bool heterogeneous = false;
};
processor_count processors();

}  // namespace mcpplibs::os::machine
