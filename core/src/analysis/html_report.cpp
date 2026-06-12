#include "perfguardian/html_report.hpp"
#include "perfguardian/version.hpp"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace perfguardian {

// helpers

static std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

static std::string iso8601_now_html() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M UTC");
    return oss.str();
}

static const char* severity_css_class(const std::string& sev) {
    if (sev == "critical") return "sev-critical";
    if (sev == "high")     return "sev-high";
    if (sev == "medium")   return "sev-medium";
    if (sev == "low")      return "sev-low";
    return "sev-info";
}

// embedded assets

static const char* kCSS = R"css(
:root {
  --bg: #0f1117; --surface: #1a1d27; --border: #2a2d3e;
  --text: #e2e8f0; --muted: #8892a4; --accent: #6c63ff;
  --high: #f87171; --medium: #fbbf24; --low: #34d399;
  --info: #60a5fa; --critical: #ff4d4f;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: 'Segoe UI', system-ui, sans-serif; background: var(--bg);
       color: var(--text); line-height: 1.6; padding: 2rem; }
h1 { font-size: 1.8rem; color: var(--accent); margin-bottom: .25rem; }
h2 { font-size: 1.1rem; color: var(--text); margin: 2rem 0 .75rem;
     padding-bottom: .4rem; border-bottom: 1px solid var(--border); }
.subtitle { color: var(--muted); font-size: .9rem; margin-bottom: 2rem; }
.cards { display: flex; gap: 1rem; flex-wrap: wrap; margin-bottom: 2rem; }
.card { background: var(--surface); border: 1px solid var(--border);
        border-radius: 8px; padding: 1rem 1.5rem; min-width: 160px; }
.card .num { font-size: 2rem; font-weight: 700; color: var(--accent); }
.card .lbl { font-size: .8rem; color: var(--muted); text-transform: uppercase; }
table { width: 100%; border-collapse: collapse; font-size: .88rem;
        background: var(--surface); border-radius: 8px; overflow: hidden; }
th { background: var(--border); color: var(--muted); font-weight: 600;
     padding: .6rem 1rem; text-align: left; cursor: pointer; user-select: none; }
th:hover { color: var(--text); }
td { padding: .55rem 1rem; border-bottom: 1px solid var(--border); }
tr:last-child td { border-bottom: none; }
tr:hover td { background: rgba(108,99,255,.07); }
.sev-critical { color: var(--critical); font-weight: 700; }
.sev-high     { color: var(--high); font-weight: 600; }
.sev-medium   { color: var(--medium); }
.sev-low      { color: var(--low); }
.sev-info     { color: var(--info); }
.badge { display: inline-block; padding: .15rem .5rem; border-radius: 4px;
         font-size: .78rem; font-weight: 600; background: rgba(108,99,255,.15);
         color: var(--accent); }
.bar-wrap { background: var(--border); border-radius: 4px; height: 8px;
            width: 120px; display: inline-block; vertical-align: middle; }
.bar-fill { height: 8px; border-radius: 4px; background: var(--accent); }
code { font-family: 'Cascadia Code', 'Fira Code', monospace;
       background: rgba(255,255,255,.05); padding: .1rem .35rem;
       border-radius: 3px; font-size: .85em; }
.section { margin-bottom: 2.5rem; }
)css";

static const char* kJS = R"js(
(function(){
  document.querySelectorAll('table').forEach(function(tbl){
    var ths = tbl.querySelectorAll('th');
    var dir = {};
    ths.forEach(function(th, ci){
      th.addEventListener('click', function(){
        var tbody = tbl.querySelector('tbody');
        var rows  = Array.from(tbody.rows);
        dir[ci]   = !dir[ci];
        rows.sort(function(a,b){
          var av = a.cells[ci] ? a.cells[ci].innerText : '';
          var bv = b.cells[ci] ? b.cells[ci].innerText : '';
          var an = parseFloat(av), bn = parseFloat(bv);
          if(!isNaN(an) && !isNaN(bn)) return dir[ci] ? an-bn : bn-an;
          return dir[ci] ? av.localeCompare(bv) : bv.localeCompare(av);
        });
        rows.forEach(function(r){ tbody.appendChild(r); });
      });
    });
  });
})();
)js";

// builder

std::string to_html_string(const AnalysisReport& report,
                            const DiagnosticSink& sink) {
    // Count diagnostics by severity
    std::map<std::string, int> sev_counts;
    for (const auto& d : sink.all()) {
        sev_counts[std::string(to_string(d.severity))]++;
    }

    // Max score for bar chart scaling
    int max_fn_score = 1;
    for (const auto& fh : report.top_functions) {
        if (fh.score > max_fn_score) max_fn_score = fh.score;
    }

    std::ostringstream o;

    // head
    o << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
      << "<meta charset=\"UTF-8\">\n"
      << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
      << "<title>PerfGuardian Report</title>\n"
      << "<style>" << kCSS << "</style>\n"
      << "</head>\n<body>\n";

    // header
    o << "<h1>PerfGuardian " << html_escape(version_str) << "</h1>\n"
      << "<p class=\"subtitle\">Generated " << iso8601_now_html() << "</p>\n";

    // summary cards
    o << "<div class=\"cards\">\n";
    auto card = [&](const std::string& num, const std::string& label) {
        o << "  <div class=\"card\"><div class=\"num\">" << html_escape(num)
          << "</div><div class=\"lbl\">" << html_escape(label) << "</div></div>\n";
    };
    card(std::to_string(report.total_issues),         "Total Issues");
    card(std::to_string(report.total_severity_score), "Severity Score");
    for (auto& [sev, cnt] : sev_counts) {
        card(std::to_string(cnt), sev);
    }
    o << "</div>\n";

    // by rule
    o << "<div class=\"section\">\n<h2>Issues by Rule</h2>\n"
      << "<table><thead><tr>"
      << "<th>Rule</th><th>Name</th><th>Count</th>"
      << "</tr></thead><tbody>\n";
    for (const auto& rs : report.rule_summary) {
        o << "<tr><td><span class=\"badge\">" << html_escape(rs.rule_id)
          << "</span></td><td>" << html_escape(rs.rule_name)
          << "</td><td>" << rs.count << "</td></tr>\n";
    }
    o << "</tbody></table></div>\n";

    // top functions
    o << "<div class=\"section\">\n<h2>Top Functions</h2>\n"
      << "<table><thead><tr>"
      << "<th>Function</th><th>File</th><th>Issues</th><th>Score</th><th>Rules</th>"
      << "</tr></thead><tbody>\n";
    for (const auto& fh : report.top_functions) {
        int pct = static_cast<int>(100.0 * fh.score / max_fn_score);
        o << "<tr><td><code>" << html_escape(fh.function_name) << "</code></td>"
          << "<td>" << html_escape(fs::path(fh.file).filename().string())
          << ":" << fh.line << "</td>"
          << "<td>" << fh.issue_count << "</td>"
          << "<td><span class=\"bar-wrap\"><span class=\"bar-fill\" style=\"width:"
          << pct << "%\"></span></span> " << fh.score << "</td><td>";
        for (const auto& rid : fh.rule_ids) {
            o << "<span class=\"badge\">" << html_escape(rid) << "</span> ";
        }
        o << "</td></tr>\n";
    }
    o << "</tbody></table></div>\n";

    // top files
    o << "<div class=\"section\">\n<h2>Top Files</h2>\n"
      << "<table><thead><tr>"
      << "<th>File</th><th>Issues</th><th>Score</th>"
      << "</tr></thead><tbody>\n";
    for (const auto& fh : report.top_files) {
        o << "<tr><td><code>" << html_escape(fh.file) << "</code></td>"
          << "<td>" << fh.issue_count << "</td>"
          << "<td>" << fh.score << "</td></tr>\n";
    }
    o << "</tbody></table></div>\n";

    // all diagnostics
    o << "<div class=\"section\">\n<h2>All Issues (" << report.total_issues << ")</h2>\n"
      << "<table><thead><tr>"
      << "<th>Rule</th><th>Severity</th><th>Function</th>"
      << "<th>Location</th><th>Message</th><th>Fix</th>"
      << "</tr></thead><tbody>\n";
    for (const auto& d : sink.all()) {
        std::string sev_str = std::string(to_string(d.severity));
        o << "<tr>"
          << "<td><span class=\"badge\">" << html_escape(d.rule_id) << "</span></td>"
          << "<td><span class=\"" << severity_css_class(sev_str) << "\">"
          << html_escape(sev_str) << "</span></td>"
          << "<td><code>" << html_escape(d.function_name) << "</code></td>"
          << "<td>" << html_escape(fs::path(d.location.file).filename().string())
          << ":" << d.location.line << "</td>"
          << "<td>" << html_escape(d.message) << "</td>"
          << "<td><code>" << html_escape(d.suggested_fix) << "</code></td>"
          << "</tr>\n";
    }
    o << "</tbody></table></div>\n";

    // footer + JS
    o << "<p style=\"color:var(--muted);font-size:.8rem;margin-top:2rem\">"
      << "PerfGuardian " << html_escape(version_str)
      << " &mdash; <a href=\"https://github.com/your-org/perfguardian\""
      << " style=\"color:var(--accent)\">GitHub</a></p>\n"
      << "<script>" << kJS << "</script>\n"
      << "</body>\n</html>\n";

    return o.str();
}

void write_html_report(const std::string& path,
                       const AnalysisReport& report,
                       const DiagnosticSink& sink) {
    std::string content = to_html_string(report, sink);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open HTML report file: " + path);
    }
    out << content;
    if (!out) {
        throw std::runtime_error("Failed to write HTML report to: " + path);
    }
}

}  // namespace perfguardian
