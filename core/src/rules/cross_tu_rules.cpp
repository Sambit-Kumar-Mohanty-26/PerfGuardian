#include "perfguardian/cross_tu_rules.hpp"
#include <string>
#include <unordered_set>

namespace perfguardian {

// PG007 — hot-pass-by-value. A function that copies a large object by value is
// already a local smell (PG001), but when that function is called from many
// sites across several files the copy cost is multiplied program-wide and the
// single signature fix pays off everywhere. The caller count and file spread
// come from the call graph, so this finding cannot be produced from one file.
static void run_pg007(const GlobalSymbolIndex& index, const CallGraph& graph,
                      DiagnosticSink& sink, const RuleConfig& cfg) {
    const long long size_threshold = cfg.pg001_size_threshold_bytes;

    for (const auto& [usr, sym] : index.all()) {
        if (!sym.is_definition) continue;
        if (sym.max_param_size <= size_threshold) continue;

        const auto callers = graph.callers_of(usr);
        if (static_cast<int>(callers.size()) < cfg.pg007_min_callers) continue;

        std::unordered_set<std::string> files;
        for (const auto& caller : callers)
            if (const auto* cs = index.find(caller)) files.insert(cs->file);
        if (static_cast<int>(files.size()) < cfg.pg007_min_caller_files) continue;

        const std::string pname =
            sym.max_param_name.empty() ? "(unnamed)" : sym.max_param_name;

        Diagnostic d;
        d.rule_id    = "PG007";
        d.rule_name  = "hot-pass-by-value";
        d.severity   = Severity::High;
        d.confidence = Confidence::High;
        d.location   = {sym.file, sym.line, 0};
        d.function_name = sym.qualified_name;
        d.message = "Parameter '" + pname + "' of type '" + sym.max_param_type +
                    "' is passed by value (" + std::to_string(sym.max_param_size) +
                    " bytes) in '" + sym.qualified_name + "', which is called from " +
                    std::to_string(callers.size()) + " functions across " +
                    std::to_string(files.size()) + " files — every call copies it; "
                    "use 'const " + sym.max_param_type + "&' to fix them all at once";
        d.suggested_fix = "const " + sym.max_param_type + "& " + pname;
        d.metrics.type_size_bytes = sym.max_param_size;
        d.metrics.copy_cost_bytes = sym.max_param_size;
        d.metrics.lookup_count    = static_cast<int>(callers.size());

        sink.emit(std::move(d));
    }
}

void run_cross_tu_rules(const GlobalSymbolIndex& index, const CallGraph& graph,
                        DiagnosticSink& sink, const RuleConfig& cfg) {
    run_pg007(index, graph, sink, cfg);
}

}  // namespace perfguardian
