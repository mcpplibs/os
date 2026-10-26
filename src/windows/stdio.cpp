// The C runtime's output descriptor saved (its handle not inherited) and
// replaced by the error one; `_dup2' to a standard descriptor also sets the
// process's standard handle, which a started program receives.
module;
#include "win32.h"

module mcpplibs.os.stdio;

import std;

namespace mcpplibs::os::stdio::sys {

int divert_output() {
    const int saved = _dup(1);
    if (saved >= 0) {
        if (const long long h = _get_osfhandle(saved); h != -1)
            SetHandleInformation(reinterpret_cast<okos_handle>(h), 0x1 /* HANDLE_FLAG_INHERIT */, 0);
        _dup2(2, 1);
    }
    return saved;
}

void restore_output(int saved) {
    _dup2(saved, 1);
    _close(saved);
}

bool write_descriptor(int descriptor, std::string_view bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const int n = _write(descriptor, bytes.data() + done, static_cast<unsigned>(bytes.size() - done));
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

}  // namespace mcpplibs::os::stdio::sys
