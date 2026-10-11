#pragma once

// A minimal check macro for the unit tests: no third-party framework, no exceptions.

#include <cstdio>

namespace crisp::test {

inline int& FailureCount() {
    static int count = 0;
    return count;
}

// The process exit code: 0 when every CHECK passed.
inline int Result() {
    return FailureCount() == 0 ? 0 : 1;
}

}  // namespace crisp::test

#define CHECK(condition)                                                                 \
    do {                                                                                 \
        if (!(condition)) {                                                              \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++crisp::test::FailureCount();                                               \
        }                                                                                \
    } while (false)
