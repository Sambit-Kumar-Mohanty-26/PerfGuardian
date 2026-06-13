#pragma once
#include <string>

namespace perfguardian {

inline constexpr int version_major = 0;
inline constexpr int version_minor = 2;
inline constexpr int version_patch = 0;
inline constexpr const char* version_str = "0.2.0";

inline std::string version_string() {
    return std::string("PerfGuardian ") + version_str;
}

}  // namespace perfguardian
