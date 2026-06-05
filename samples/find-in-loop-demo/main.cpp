#include <vector>
#include <algorithm>
#include <map>
#include <string>

// BAD: std::find inside a loop — O(N^2) — PG004 should fire
bool hasCommonElement(const std::vector<int>& a, const std::vector<int>& b) {
    for (int x : a) {
        if (std::find(b.begin(), b.end(), x) != b.end()) {  // O(N) inside O(N)
            return true;
        }
    }
    return false;
}

// GOOD: sort + set intersection — O(N log N) — PG004 must NOT fire
bool hasCommonElementFast(std::vector<int> a, std::vector<int> b) {
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    std::vector<int> common;
    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(common));
    return !common.empty();
}

// BAD: same map key looked up twice — PG006 should fire
void processMap(std::map<std::string, int>& m, const std::string& key) {
    if (m.count(key) > 0) {
        m[key] += 1;  // second lookup of same key
    }
}

// GOOD: single lookup via find — PG006 must NOT fire
void processMapFast(std::map<std::string, int>& m, const std::string& key) {
    auto it = m.find(key);
    if (it != m.end()) {
        it->second += 1;
    }
}

int main() {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b = {4, 5, 3};
    hasCommonElement(a, b);
    hasCommonElementFast(a, b);

    std::map<std::string, int> m = {{"x", 1}};
    processMap(m, "x");
    processMapFast(m, "x");
    return 0;
}
