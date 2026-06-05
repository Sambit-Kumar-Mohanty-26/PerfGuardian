#include <vector>
#include <string>

// BAD: no reserve() before a known-size loop — PG003 should fire
void buildVectorBad(int n) {
    std::vector<int> v;
    for (int i = 0; i < n; ++i) {
        v.push_back(i);  // repeated reallocations
    }
}

// GOOD: reserve before loop — PG003 must NOT fire
void buildVectorGood(int n) {
    std::vector<int> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) {
        v.push_back(i);
    }
}

// BAD: string concatenation in loop — PG005 should fire
std::string buildStringBad(int n) {
    std::string result;
    for (int i = 0; i < n; ++i) {
        result = result + "x";  // creates a temporary each iteration
    }
    return result;
}

// GOOD: += avoids temporaries — PG005 must NOT fire
std::string buildStringGood(int n) {
    std::string result;
    result.reserve(n);
    for (int i = 0; i < n; ++i) {
        result += "x";
    }
    return result;
}

int main() {
    buildVectorBad(1000);
    buildVectorGood(1000);
    buildStringBad(100);
    buildStringGood(100);
    return 0;
}
