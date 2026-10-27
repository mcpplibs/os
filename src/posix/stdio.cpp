// The process's output descriptor saved (closed on exec, so not inherited) and
// replaced by the error one, by the kernel's calls (src/posix/kernel.h): what
// openkal's standard output, the C library's and a started program's all are.
module;
#include "kernel.h"

module mcpplibs.os.stdio;

import std;

namespace mcpplibs::os::stdio::sys {

namespace k = mcpplibs::os::kernel;

int divert_output() {
    const long saved = k::dup_above(1, 3);
    if (saved < 0) return -1;
    k::dup_onto(2, 1);
    return static_cast<int>(saved);
}

void restore_output(int saved) {
    k::dup_onto(saved, 1);
    k::close(saved);
}

bool write_descriptor(int descriptor, std::string_view bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const long n = k::write(descriptor, bytes.data() + done, bytes.size() - done);
        if (n == -4 /* EINTR */) continue;
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

}  // namespace mcpplibs::os::stdio::sys
