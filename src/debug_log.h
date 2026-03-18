#pragma once

#include <cstdlib>
#include <cstdio>

namespace mc {
namespace runtime {

inline bool DebugLogEnabled() {
    const char* v = std::getenv("MATX_DEBUG_LOG");
    return v != nullptr && v[0] != '\0' && v[0] != '0';
}

}  // namespace runtime
}  // namespace mc

#define MC_DLOG_STREAM(expr) \
    do {                     \
        if (::mc::runtime::DebugLogEnabled()) { expr; } \
    } while (0)

#define MC_DLOG_PRINTF(...)  \
    do {                     \
        if (::mc::runtime::DebugLogEnabled()) { std::printf(__VA_ARGS__); } \
    } while (0)
