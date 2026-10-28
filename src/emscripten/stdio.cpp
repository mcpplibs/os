// The output diverted where the streams are: Emscripten's C library keeps the
// descriptors openkal-emscripten's standard streams borrow, so its `dup' and
// `dup2' move what both write to.
module;
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

module mcpplibs.os.stdio;

import std;

namespace mcpplibs::os::stdio::sys {

int divert_output() {
    const int saved = ::fcntl(1, F_DUPFD_CLOEXEC, 3);
    if (saved < 0) return -1;
    if (::dup2(2, 1) < 0) {
        ::close(saved);
        return -1;
    }
    return saved;
}

void restore_output(int saved) {
    ::dup2(saved, 1);
    ::close(saved);
}

bool write_descriptor(int descriptor, std::string_view bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const auto n = ::write(descriptor, bytes.data() + done, bytes.size() - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

}  // namespace mcpplibs::os::stdio::sys
