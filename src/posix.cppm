// mcpplibs.os.posix --- what Linux and macOS share and Windows does not (R5).
export module mcpplibs.os.posix;

import std;

export namespace mcpplibs::os::posix {

// The effective user's number; zero is the superuser.
std::uint32_t effective_user();

}  // namespace mcpplibs::os::posix
