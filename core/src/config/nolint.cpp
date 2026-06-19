#include "perfguardian/nolint.hpp"
#include <spdlog/spdlog.h>
#include <cctype>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace perfguardian {

namespace {

// Is `rule` listed in a comma-separated "(PG001,PG002)" body (whitespace-tolerant)?
bool id_list_contains(const std::string& ids, const std::string& rule) {
    std::size_t i = 0;
    while (i < ids.size()) {
        std::size_t comma = ids.find(',', i);
        std::size_t end = (comma == std::string::npos) ? ids.size() : comma;
        std::size_t a = i, b = end;
        while (a < b && std::isspace(static_cast<unsigned char>(ids[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(ids[b - 1]))) --b;
        if (ids.compare(a, b - a, rule) == 0) return true;
        if (comma == std::string::npos) break;
        i = comma + 1;
    }
    return false;
}

std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(path, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

}  // namespace

bool nolint_line_suppresses(const std::string& line, const std::string& rule,
                            bool nextline) {
    static const std::string kTok = "NOLINT";
    std::size_t pos = 0;
    while ((pos = line.find(kTok, pos)) != std::string::npos) {
        std::size_t after = pos + kTok.size();
        const bool is_next = line.compare(after, 8, "NEXTLINE") == 0;
        std::size_t paren = after + (is_next ? 8 : 0);

        if (is_next != nextline) { pos = after; continue; }

        // No parenthesized rule list → matches every rule.
        if (paren >= line.size() || line[paren] != '(') return true;
        std::size_t close = line.find(')', paren);
        std::string ids = line.substr(
            paren + 1, close == std::string::npos ? std::string::npos : close - paren - 1);
        if (id_list_contains(ids, rule)) return true;
        pos = after;
    }
    return false;
}

void apply_nolint_suppressions(DiagnosticSink& sink) {
    // Cache each referenced file's lines so every file is read at most once.
    std::map<std::string, std::vector<std::string>> file_lines;
    auto lines_for = [&](const std::string& f) -> const std::vector<std::string>& {
        auto it = file_lines.find(f);
        if (it == file_lines.end()) it = file_lines.emplace(f, read_lines(f)).first;
        return it->second;
    };

    int removed = 0;
    sink.remove_if([&](const Diagnostic& d) {
        const int ln = d.location.line;
        if (ln <= 0 || d.location.file.empty()) return false;
        const auto& lines = lines_for(d.location.file);

        // Same line: a trailing NOLINT.
        if (ln <= static_cast<int>(lines.size()) &&
            nolint_line_suppresses(lines[ln - 1], d.rule_id, false)) {
            ++removed; return true;
        }
        // Preceding line: a NOLINTNEXTLINE.
        if (ln - 1 >= 1 && ln - 1 <= static_cast<int>(lines.size()) &&
            nolint_line_suppresses(lines[ln - 2], d.rule_id, true)) {
            ++removed; return true;
        }
        return false;
    });

    if (removed > 0) spdlog::debug("NOLINT: suppressed {} finding(s)", removed);
}

}  // namespace perfguardian
