#pragma once

// Deliberately large struct — 800 bytes — for PG001 rule testing.
// PerfGuardian should flag any function accepting Player by value.
struct Player {
    char name[64];
    char data[736];  // padding to reach 800 bytes total
};
