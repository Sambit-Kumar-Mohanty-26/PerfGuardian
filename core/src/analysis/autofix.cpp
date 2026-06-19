#include "perfguardian/autofix.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace perfguardian {

namespace {

// One edit resolved to absolute byte offsets within a file.
struct Edit {
    std::size_t start = 0;
    std::size_t end   = 0;          // half-open [start, end)
    std::string replacement;
};

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Byte offset of the start of each 1-based line.
std::vector<std::size_t> line_offsets(const std::string& content) {
    std::vector<std::size_t> starts{0};
    for (std::size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\n') starts.push_back(i + 1);
    }
    return starts;
}

}  // namespace

FixSummary apply_fixits(const std::vector<Diagnostic>& diags) {
    // Collect valid fixits per file.
    std::map<std::string, std::vector<FixIt>> by_file;
    for (const auto& d : diags) {
        for (const auto& fx : d.fixits) {
            if (fx.valid()) by_file[d.location.file].push_back(fx);
        }
    }

    FixSummary summary;

    for (auto& [path, fixits] : by_file) {
        std::string content = read_file(path);
        if (content.empty()) { summary.skipped += static_cast<int>(fixits.size()); continue; }
        const auto starts = line_offsets(content);

        // Resolve each fixit to absolute offsets, dropping any out of range.
        std::vector<Edit> edits;
        for (const auto& fx : fixits) {
            if (fx.start_line <= 0 || static_cast<std::size_t>(fx.start_line) > starts.size())
                { ++summary.skipped; continue; }
            std::size_t s = starts[fx.start_line - 1] + (fx.start_col - 1);
            std::size_t e = starts[fx.end_line   - 1] + (fx.end_col   - 1);
            if (s >= e || e > content.size()) { ++summary.skipped; continue; }
            edits.push_back({s, e, fx.replacement});
        }

        // Drop exact duplicates (same span from a header seen in many TUs).
        std::sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) {
            if (a.start != b.start) return a.start < b.start;
            return a.end < b.end;
        });
        edits.erase(std::unique(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) {
            return a.start == b.start && a.end == b.end && a.replacement == b.replacement;
        }), edits.end());

        // Apply highest offset first; skip any edit overlapping one already kept.
        std::sort(edits.begin(), edits.end(),
                  [](const Edit& a, const Edit& b) { return a.start > b.start; });
        std::size_t last_start = content.size() + 1;  // lowest start applied so far
        int applied_here = 0;
        for (const auto& ed : edits) {
            if (ed.end > last_start) { ++summary.skipped; continue; }  // overlaps a later edit
            content.replace(ed.start, ed.end - ed.start, ed.replacement);
            last_start = ed.start;
            ++applied_here;
        }

        if (applied_here > 0) {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << content;
            summary.applied += applied_here;
            summary.files.push_back(path);
            spdlog::debug("autofix: applied {} edit(s) to {}", applied_here, path);
        }
    }

    return summary;
}

}  // namespace perfguardian
