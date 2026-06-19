#pragma once
#include "perfguardian/diagnostic.hpp"

namespace perfguardian {

// Remove diagnostics suppressed by inline comments in the source, clang-tidy
// style:
//   foo(Big b);                 // NOLINT              - suppress all on this line
//   foo(Big b);                 // NOLINT(PG001)       - suppress PG001 on this line
//   foo(Big b);                 // NOLINT(PG001,PG002) - suppress either
//   // NOLINTNEXTLINE(PG001)
//   foo(Big b);                                         - suppress PG001 next line
// A bare NOLINT (no parentheses) matches every rule. Files are read once.
void apply_nolint_suppressions(DiagnosticSink& sink);

// Exposed for testing: does `line` carry a NOLINT directive that suppresses
// `rule_id`? `nextline` selects NOLINTNEXTLINE vs the same-line NOLINT.
bool nolint_line_suppresses(const std::string& line,
                            const std::string& rule_id,
                            bool nextline);

}  // namespace perfguardian
