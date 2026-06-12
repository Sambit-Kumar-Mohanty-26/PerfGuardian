#pragma once
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "perfguardian/severity.hpp"

namespace perfguardian {

struct Location {
    std::string file;
    int line   = 0;
    int column = 0;
};

// Quantitative metrics attached to a diagnostic.
// All values are optional; rules fill only what they can measure.
struct Metrics {
    long long type_size_bytes    = 0;   // sizeof the offending type
    long long copy_cost_bytes    = 0;   // bytes copied per call site invocation
    int       loop_depth         = 0;   // nesting depth that multiplies cost
    int       lookup_count       = 0;   // redundant lookups detected
    std::string complexity_before;      // e.g. "O(N^2)"
    std::string complexity_after;       // e.g. "O(N log N)"
};

struct Diagnostic {
    std::string rule_id;          // e.g. "PG001"
    std::string rule_name;        // e.g. "large-object-by-value"
    Severity    severity = Severity::Medium;
    Location    location;
    std::string function_name;    // qualified name of the enclosing function
    std::string message;          // human-readable explanation
    std::string suggested_fix;    // e.g. "const Player&"
    std::string code_snippet;     // source context (optional)
    Metrics     metrics;
    bool        suppressed = false;
};

// Collects diagnostics emitted by rules during analysis.
class DiagnosticSink {
public:
    void emit(Diagnostic d) {
        if (!d.suppressed) {
            diagnostics_.push_back(std::move(d));
        }
    }

    const std::vector<Diagnostic>& all() const { return diagnostics_; }

    std::vector<Diagnostic> with_severity(Severity min_severity) const {
        std::vector<Diagnostic> result;
        for (const auto& d : diagnostics_) {
            if (severity_weight(d.severity) >= severity_weight(min_severity)) {
                result.push_back(d);
            }
        }
        return result;
    }

    // Remove all diagnostics that satisfy pred (used for suppression).
    void remove_if(std::function<bool(const Diagnostic&)> pred) {
        diagnostics_.erase(
            std::remove_if(diagnostics_.begin(), diagnostics_.end(), pred),
            diagnostics_.end());
    }

    std::size_t count() const { return diagnostics_.size(); }
    bool empty() const { return diagnostics_.empty(); }

private:
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace perfguardian
