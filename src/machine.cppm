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

}  // namespace mcpplibs::os::machine
