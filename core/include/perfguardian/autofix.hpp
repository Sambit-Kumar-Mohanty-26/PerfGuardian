#pragma once
#include "perfguardian/diagnostic.hpp"
#include <string>
#include <vector>

namespace perfguardian {

struct FixSummary {
    int applied = 0;                    // edits written
    int skipped = 0;                    // edits dropped (overlap / out of range)
    std::vector<std::string> files;     // files modified
};

// Apply every valid FixIt carried by `diags` to the source files on disk.
// Edits are applied highest-offset-first per file so earlier positions stay
// valid; overlapping or duplicate edits are skipped rather than corrupting the
// file. Files with no applicable edit are left untouched.
FixSummary apply_fixits(const std::vector<Diagnostic>& diags);

}  // namespace perfguardian
