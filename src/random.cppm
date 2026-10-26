// mcpplibs.os.random --- bytes no other party can predict, from the system's
// own source (openkal.random): a token a peer must present, a name another
// program cannot guess. Not a generator: what is wanted many times over is
// seeded from here, by the program, with the algorithm it chooses.
export module mcpplibs.os.random;

import std;
export import mcpplibs.os.error;

export namespace mcpplibs::os::random {

// Every byte of `out', or none (errc::again while a source early in boot is
// not yet seeded).
result<void> fill(std::span<std::byte> out);

// `n' such bytes as lowercase hexadecimal digits.
result<std::string> hex(std::size_t n);

}  // namespace mcpplibs::os::random
