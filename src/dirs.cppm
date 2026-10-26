// mcpplibs.os.dirs --- where a program keeps what it keeps, by the conventions
// of the system it runs upon.
//
//              Linux (XDG)               macOS                          Windows
//   home       $HOME                     $HOME                          %USERPROFILE%
//   config     $XDG_CONFIG_HOME|~/.config ~/Library/Application Support %APPDATA%
//   data       $XDG_DATA_HOME|~/.local/share ~/Library/Application Support %LOCALAPPDATA%
//   state      $XDG_STATE_HOME|~/.local/state ~/Library/Application Support %LOCALAPPDATA%
//   cache      $XDG_CACHE_HOME|~/.cache  ~/Library/Caches               %LOCALAPPDATA%
//   runtime    $XDG_RUNTIME_DIR|temp     $TMPDIR                        %TEMP%
//   temp       $TMPDIR|/tmp              $TMPDIR|/tmp                   %TEMP%|%TMP%
//
// The system is `host()', so an image follows the conventions of the system it
// was loaded on. A variable that is set and empty is treated as not set, as the
// XDG specification says.
export module mcpplibs.os.dirs;

import std;

export namespace mcpplibs::os::dirs {

std::optional<std::filesystem::path> home();
std::optional<std::filesystem::path> config();
std::optional<std::filesystem::path> data();
std::optional<std::filesystem::path> state();
std::optional<std::filesystem::path> cache();
std::optional<std::filesystem::path> runtime();
std::filesystem::path temp();

}  // namespace mcpplibs::os::dirs
