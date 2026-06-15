#pragma once
#include "perfguardian/symbol_index.hpp"
#include "perfguardian/call_graph.hpp"
#include "perfguardian/diagnostic.hpp"
#include "perfguardian/rules.hpp"

namespace perfguardian {

// Rules that reason across translation units, using the global symbol index and
// the call graph rather than a single TU's SymbolDB. Emits into `sink`.
//
// PG007 (hot-pass-by-value): a function with a large copyable by-value parameter
// that is called from many sites across multiple files — the per-call copy cost
// is multiplied by the whole-program call count, which no single-file pass can see.
void run_cross_tu_rules(const GlobalSymbolIndex& index,
                        const CallGraph& graph,
                        DiagnosticSink& sink,
                        const RuleConfig& cfg = {});

}  // namespace perfguardian
