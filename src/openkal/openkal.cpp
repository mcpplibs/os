// mcpplibs.os in an ok image (`<arch>-openkal'): what the native targets know
// when they are compiled, an image learns when it is loaded.
//
//   host()           the system the loader runs upon, by the name openkal
//                    gives it (`kal_env_system', SPEC clause 11 entry 30) ---
//                    for conventions only, as openkal restricts it;
//   machine::arch()  the architecture the image was built for, which is the
//                    one it runs upon: a loader runs images of its own;
//   machine::memory() not known: no openkal operation reports it, and an
//                    answer guessed here would be wrong somewhere.
module;
#include <openkal/env.h>

module mcpplibs.os.platform;

import std;

namespace mcpplibs::os {

platform host() noexcept {
    static const platform answer = [] {
        char name[32] = {};
        const kal_intptr n = kal_env_system(name, sizeof name - 1);
        const std::string_view s(name, n > 0 ? static_cast<std::size_t>(n) : 0);
        if (s == "windows") return platform::windows;
        if (s == "macos") return platform::macos;
        return platform::linux;
    }();
    return answer;
}

}  // namespace mcpplibs::os
