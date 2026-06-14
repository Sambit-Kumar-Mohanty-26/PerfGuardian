#pragma once
#include <string_view>
#include <stdexcept>

namespace perfguardian {

// How sure a rule is that a finding is a true positive. Lets CI gate on
// high-confidence findings (`--min-confidence high`) while still surfacing
// more speculative ones in full reports.
enum class Confidence { Low, Medium, High };

inline std::string_view to_string(Confidence c) {
    switch (c) {
        case Confidence::Low:    return "low";
        case Confidence::Medium: return "medium";
        case Confidence::High:   return "high";
    }
    return "unknown";
}

inline std::string_view confidence_label(Confidence c) {
    switch (c) {
        case Confidence::Low:    return "LOW";
        case Confidence::Medium: return "MEDIUM";
        case Confidence::High:   return "HIGH";
    }
    return "UNKNOWN";
}

inline Confidence confidence_from_string(std::string_view s) {
    if (s == "low")    return Confidence::Low;
    if (s == "medium") return Confidence::Medium;
    if (s == "high")   return Confidence::High;
    throw std::invalid_argument(std::string("Unknown confidence: ") + std::string(s));
}

inline int confidence_weight(Confidence c) {
    switch (c) {
        case Confidence::Low:    return 1;
        case Confidence::Medium: return 2;
        case Confidence::High:   return 3;
    }
    return 0;
}

}  // namespace perfguardian
