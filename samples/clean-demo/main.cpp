// This file contains no performance issues.
// PerfGuardian must produce zero diagnostics against this file.
// Used as the false-positive test corpus.

#include <vector>
#include <string>
#include <algorithm>
#include <map>

struct SmallPoint {
    float x, y;  // 8 bytes — below the size threshold
};

// Small struct by value is fine
void processPoint(SmallPoint p) {
    (void)p;
}

// Const ref for large-ish objects
struct Config {
    char data[256];
};
void applyConfig(const Config& cfg) {
    (void)cfg;
}

// reserve() present — no PG003
void buildVector(int n) {
    std::vector<int> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
    }
}

// Single map lookup — no PG006
void updateMap(std::map<std::string, int>& m, const std::string& k) {
    auto it = m.find(k);
    if (it != m.end()) {
        it->second++;
    }
}

int main() {
    processPoint({1.0f, 2.0f});
    Config cfg{};
    applyConfig(cfg);
    buildVector(100);
    std::map<std::string, int> m;
    updateMap(m, "key");
    return 0;
}
