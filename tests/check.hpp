// A test reports each observation and exits with the number that did not hold.
#pragma once
#include <cstdio>
inline int g_failures = 0;
inline void check(bool held, const char* what) {
    std::printf("  %s  %s\n", held ? "held" : "FAIL", what);
    if (!held) ++g_failures;
}
