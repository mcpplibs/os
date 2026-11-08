// The image replaced (execve), on Linux and macOS: the identity, the terminal
// and the place among the processes that wait for this one stay; the status a
// waiter sees is the program's.
module;
#include <cerrno>
#include <unistd.h>
#include "kernel.h"

module mcpplibs.os.process;

import std;
import mcpplibs.os.error;
import mcpplibs.os.platform;

namespace mcpplibs::os::process::sys {

result<usage> resources(of who) {
    long r[18] = {};
    if (kernel::resource_usage(who == of::self ? 0 : -1, r) < 0)
        return std::unexpected(error{ errc::io, "asking the kernel what was used" });
    usage u;
    u.user = std::chrono::seconds(r[0]) + std::chrono::microseconds(r[1]);
    u.system = std::chrono::seconds(r[2]) + std::chrono::microseconds(r[3]);
    u.peak_memory = static_cast<std::uint64_t>(r[4]) * kernel::maxrss_unit();
    // The bytes moved by reads and writes, where the kernel counts them for
    // this process: /proc/self/io on Linux. The children's are not counted.
    if (who == of::self)
        if (std::ifstream io("/proc/self/io"); io) {
            std::string key;
            std::uint64_t value = 0;
            while (io >> key >> value) {
                if (key == "rchar:") u.read_bytes = value;
                else if (key == "wchar:") u.written_bytes = value;
            }
        }
    return u;
}

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
    // SIGPIPE AT ITS DEFAULT, AS A SHELL STARTS A PROGRAM. openkal ignores it in
    // this program so that a write to a closed reader is reported rather than
    // fatal, and an ignored disposition survives execve: the program this one
    // becomes would begin with it ignored, and `yes | head' inside it would
    // report a write error instead of ending. A handler is reset by execve, an
    // ignore is not, so the ignore is undone here, in the kernel, where the
    // process keeps it (src/posix/kernel.h).
    mcpplibs::os::kernel::default_signal(mcpplibs::os::kernel::sigpipe);
    ::execve(program.c_str(), args.data(), envp.data());
    const int e = errno;
    // A file this machine cannot start: no image at all (ENOEXEC), or on
    // macOS an image of another processor (EBADARCH 86, EBADMACHO 88), as
    // openkal reports a refused start.
    const bool not_program = e == ENOEXEC || (is_macos && (e == 86 || e == 88));
    return std::unexpected(error{ not_program ? errc::not_program
                                : e == ENOENT ? errc::not_found : e == EACCES ? errc::permission : errc::io,
        std::format("becoming {}: {}", program.generic_string(), std::generic_category().message(e)) });
}

}  // namespace mcpplibs::os::process::sys
