#include "perfguardian/sarif_report.hpp"
#include "perfguardian/version.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace perfguardian {

// helpers

static const char* sarif_level(Severity s) {
    switch (s) {
        case Severity::Critical: return "error";
        case Severity::High:     return "error";
        case Severity::Medium:   return "warning";
        case Severity::Low:      return "note";
        case Severity::Info:     return "none";
    }
    return "warning";
}

// Make a file URI relative to repo_root when possible.
static std::string make_uri(const std::string& file_path,
                             const std::string& repo_root) {
    if (repo_root.empty() || file_path.empty()) return file_path;
    std::error_code ec;
    fs::path rel = fs::relative(file_path, repo_root, ec);
    if (!ec) {
        // SARIF URIs use forward slashes
        std::string s = rel.generic_string();
        return s;
    }
    return file_path;
}

// rule descriptors (driver.rules)

struct RuleDescriptor {
    const char* id;
    const char* name;
    const char* short_desc;
    const char* full_desc;
};

static const RuleDescriptor kRules[] = {
    { "PG001", "large-object-by-value",
      "Pass large objects by const reference instead of by value.",
      "Passing a large type by value incurs a copy at every call site. "
      "Prefer `const T&` for read-only parameters." },
    { "PG002", "non-const-reference-param",
      "Non-const reference parameter may signal unintended mutation.",
      "A non-const reference parameter larger than the threshold may be "
      "an accidental omission of `const`. Add `const` if the callee does "
      "not modify the argument." },
    { "PG003", "push-back-in-loop",
      "Calling push_back/emplace_back in a loop without reserve().",
      "Repeated push_back inside a loop triggers O(N) reallocations. "
      "Call vector::reserve() before the loop when the final size is known." },
    { "PG004", "associative-lookup-in-loop",
      "Associative container lookup inside a loop.",
      "Repeated find/count/contains calls in a loop have O(N log N) or "
      "O(N) cost. Hoist the lookup or cache the result." },
    { "PG005", "large-local-by-value",
      "Large type captured as a local variable by value.",
      "A local variable holding a large type by value costs stack space and "
      "copy construction. Consider using a reference or moving into the variable." },
    { "PG006", "redundant-map-lookup",
      "Map key looked up multiple times consecutively.",
      "Calling find/at/operator[] on the same key more than once performs "
      "redundant tree traversals. Cache the result of the first lookup." },
};

static json build_rules_array() {
    json arr = json::array();
    for (const auto& r : kRules) {
        arr.push_back({
            { "id",   r.id   },
            { "name", r.name },
            { "shortDescription", { { "text", r.short_desc } } },
            { "fullDescription",  { { "text", r.full_desc  } } },
            { "helpUri", "https://github.com/your-org/perfguardian/wiki/rules/" + std::string(r.id) },
            { "properties", { { "tags", json::array({"performance", "cpp"}) } } },
        });
    }
    return arr;
}

// main builder

std::string to_sarif_string(const AnalysisReport& /*report*/,
                             const DiagnosticSink& sink,
                             const std::string& repo_root) {
    json results = json::array();
    for (const auto& d : sink.all()) {
        std::string uri = make_uri(d.location.file, repo_root);

        json loc = {
            { "physicalLocation", {
                { "artifactLocation", {
                    { "uri", uri },
                    { "uriBaseId", repo_root.empty() ? "" : "%SRCROOT%" }
                }},
                { "region", {
                    { "startLine",   d.location.line   > 0 ? d.location.line   : 1 },
                    { "startColumn", d.location.column > 0 ? d.location.column : 1 }
                }}
            }}
        };

        json result_obj = {
            { "ruleId",  d.rule_id },
            { "level",   sarif_level(d.severity) },
            { "message", { { "text", d.message } } },
            { "locations", json::array({ loc }) },
        };

        if (!d.suggested_fix.empty()) {
            result_obj["fixes"] = json::array({
                { { "description", { { "text", d.suggested_fix } } } }
            });
        }

        results.push_back(std::move(result_obj));
    }

    json sarif = {
        { "$schema", "https://json.schemastore.org/sarif-2.1.0.json" },
        { "version", "2.1.0" },
        { "runs", json::array({
            {
                { "tool", {
                    { "driver", {
                        { "name",           "PerfGuardian" },
                        { "version",        std::string(version_str) },
                        { "informationUri", "https://github.com/your-org/perfguardian" },
                        { "rules",          build_rules_array() }
                    }}
                }},
                { "results", results }
            }
        })}
    };

    return sarif.dump(2);
}

void write_sarif_report(const std::string& path,
                        const AnalysisReport& report,
                        const DiagnosticSink& sink,
                        const std::string& repo_root) {
    std::string content = to_sarif_string(report, sink, repo_root);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open SARIF report file: " + path);
    }
    out << content;
    if (!out) {
        throw std::runtime_error("Failed to write SARIF report to: " + path);
    }
}

}  // namespace perfguardian
