// mcpplibs.os.env --- the parameters this program was started with.
//
// The set is what the program was started with and does not change while it
// runs (openkal.env); a started program's environment is stated when it is
// started (`process::spawn_options'), and this program's own is not modified
// --- part 2 §6.4.
export module mcpplibs.os.env;

import std;
export import mcpplibs.os.error;

export namespace mcpplibs::os::env {

// A variable's value; nothing when it is not set, which is distinct from set
// and empty.
std::optional<std::string> get(std::string_view name);

// Every variable, as `NAME=value', in an order that does not change.
std::vector<std::string> variables();

// The arguments, the first being the name the program was started by.
std::vector<std::string> arguments();

// The image this program was started from, as the system states it (openkal's
// `kal_env_image'); for an image started by a loader, the image's.
std::optional<std::filesystem::path> image();

// The program that loaded this image and runs it, where this program is an ok
// image (`<arch>-openkal'; openkal's `kal_env_loader'); nothing for a program
// built for its environment.
std::optional<std::filesystem::path> loader();

// What starts this program again, before its own arguments: its image; in an ok
// image, the loader and then the image --- or the loader alone, where the image
// is carried inside it (a self-contained program, M3).
std::vector<std::string> self_command();

// The system the program runs upon, by name (`linux', `macos', `windows').
// For conventions only, as openkal restricts it.
std::optional<std::string> system();

// The directories `PATH' names, in order.
std::vector<std::filesystem::path> search_path();

// A program by name, as a shell would find it: a name with a directory is
// itself; a bare name is looked for in each directory of `PATH' (or the list
// given), and where the host's programs carry suffixes, with each suffix of
// `PATHEXT'.
std::optional<std::filesystem::path> find_executable(std::string_view name);
std::optional<std::filesystem::path> find_executable(std::string_view name, std::string_view path_list);

}  // namespace mcpplibs::os::env
