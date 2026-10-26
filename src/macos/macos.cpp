module;
#include <sys/sysctl.h>

module mcpplibs.os.macos;

import std;

namespace mcpplibs::os::macos {

bool translated() {
    int v = 0;
    std::size_t len = sizeof v;
    return ::sysctlbyname("sysctl.proc_translated", &v, &len, nullptr, 0) == 0 && v == 1;
}

}  // namespace mcpplibs::os::macos
