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

// The ANSI code page the narrow interfaces of this process use (GetACP): 65001
// under an application manifest that asks for UTF-8 (0.2.0).
unsigned code_page();
// The output code page of the console the stream writes to; 0 when the stream
// is not a console.
unsigned console_output_code_page(bool error_stream = false);
// Whether the system lets a program that declares itself long-path aware use
// names past MAX_PATH (LongPathsEnabled).
bool long_paths_enabled();
// A directory named by a string value under HKEY_LOCAL_MACHINE, read in the
// 64-bit view and then the 32-bit one, without a trailing separator.
std::optional<std::filesystem::path> machine_registry_path(std::wstring_view key, std::wstring_view value);
// Whether `drive' is a fixed drive on which this user can create a file, and
// `child' under its root is absent or a plain directory.
bool writable_fixed_drive(char drive, std::wstring_view child);

}  // namespace mcpplibs::os::windows
