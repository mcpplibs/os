// What Linux, macOS and Emscripten share of mcpplibs.os.fs's own, from the C
// library: durability, ownership, whether a node may be written.
module;
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

module mcpplibs.os.fs;

import std;
import mcpplibs.os.error;

namespace mcpplibs::os::fs::sys {

result<void> sync(const std::filesystem::path& node) {
    const int fd = ::open(node.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::unexpected(error{ errno == ENOENT ? errc::not_found : errc::io,
        std::format("making {} durable: {}", node.generic_string(), std::generic_category().message(errno)), errno });
    // A volume that keeps nothing durable for this node says so; it is not a failure.
    if (::fsync(fd) != 0 && errno != EINVAL && errno != EROFS && errno != ENOTSUP && errno != EOPNOTSUPP) {
        const int e = errno;
        ::close(fd);
        return std::unexpected(error{ errc::io, std::format("making {} durable: {}", node.generic_string(),
                                                            std::generic_category().message(e)), e });
    }
    ::close(fd);
    return {};
}

owner owner_of(const std::filesystem::path& node) {
    struct stat st {};
    if (::stat(node.c_str(), &st) != 0) return owner::unknown;
    if (st.st_uid == ::getuid()) return owner::this_user;
    return st.st_uid == 0 ? owner::system : owner::another_user;
}

bool writable(const std::filesystem::path& node) { return ::access(node.c_str(), W_OK) == 0; }

}  // namespace mcpplibs::os::fs::sys
