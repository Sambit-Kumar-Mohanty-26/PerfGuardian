#pragma once
#include <string_view>
#include <stdexcept>

namespace perfguardian {

enum class Severity { Info, Low, Medium, High, Critical };

inline std::string_view to_string(Severity s) {
    switch (s) {
        case Severity::Info:     return "info";
        case Severity::Low:      return "low";
        case Severity::Medium:   return "medium";
        case Severity::High:     return "high";
        case Severity::Critical: return "critical";
    }
    return "unknown";
}

inline std::string_view severity_label(Severity s) {
    switch (s) {
        case Severity::Info:     return "INFO";
        case Severity::Low:      return "LOW";
        case Severity::Medium:   return "MEDIUM";
        case Severity::High:     return "HIGH";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

inline Severity severity_from_string(std::string_view s) {
    if (s == "info")     return Severity::Info;
    if (s == "low")      return Severity::Low;
    if (s == "medium")   return Severity::Medium;
    if (s == "high")     return Severity::High;
    if (s == "critical") return Severity::Critical;
    throw std::invalid_argument(std::string("Unknown severity: ") + std::string(s));
}

inline int severity_weight(Severity s) {
    switch (s) {
        case Severity::Info:     return 1;
        case Severity::Low:      return 2;
        case Severity::Medium:   return 3;
        case Severity::High:     return 4;
        case Severity::Critical: return 5;
    }
    return 0;
}

}  // namespace perfguardian
