// The image replaced (execve), on Linux and macOS: the identity, the terminal
// and the place among the processes that wait for this one stay; the status a
// waiter sees is the program's.
module;
#include <cerrno>
#include <unistd.h>

module mcpplibs.os.process;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::process::sys {

result<void> replace_image(const std::filesystem::path& program,
                           const std::vector<std::string>& argv,
                           const std::vector<std::string>& environment,
                           const std::filesystem::path& cwd) {
    std::vector<char*> args, envp;
    for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    for (const auto& e : environment) envp.push_back(const_cast<char*>(e.c_str()));
    envp.push_back(nullptr);
    if (!cwd.empty() && ::chdir(cwd.c_str()) != 0)
        return std::unexpected(error{ errc::not_found, std::format(
            "entering {}: {}", cwd.generic_string(), std::generic_category().message(errno)) });
    ::execve(program.c_str(), args.data(), envp.data());
    const int e = errno;
    return std::unexpected(error{ e == ENOENT ? errc::not_found : e == EACCES ? errc::permission : errc::io,
        std::format("becoming {}: {}", program.generic_string(), std::generic_category().message(e)) });
}

}  // namespace mcpplibs::os::process::sys
