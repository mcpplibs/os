// mcpplibs.os.posix --- what Linux and macOS share and Windows does not (R5).
export module mcpplibs.os.posix;

import std;

export namespace mcpplibs::os::posix {

// The effective user's number; zero is the superuser.
std::uint32_t effective_user();

// The permissions every file this program creates from now on is denied
// (umask): 022 makes them readable by everyone and writable by their owner,
// what a directory shared by a machine's users is written with (0.2.0).
void set_creation_mask(std::uint32_t mask);

}  // namespace mcpplibs::os::posix
