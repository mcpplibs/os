// mcpplibs.os.windows --- what Windows has and the others do not (R5).
export module mcpplibs.os.windows;

import std;

export namespace mcpplibs::os::windows {

// Whether this program holds the administrator's token. Other systems have no
// single equivalent: their privileges are not a token a program holds.
bool elevated();

// Whether the directory is on a Dev Drive, which the system scans less and a
// build places its output on; nothing when the system cannot say.
std::optional<bool> on_dev_drive(const std::filesystem::path& directory);

}  // namespace mcpplibs::os::windows
