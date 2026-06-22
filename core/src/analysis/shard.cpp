#include "perfguardian/shard.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace perfguardian {

namespace {

json fixit_to_json(const FixIt& f) {
    return {{"sl", f.start_line}, {"sc", f.start_col},
            {"el", f.end_line},   {"ec", f.end_col}, {"rep", f.replacement}};
}
FixIt fixit_from_json(const json& j) {
    FixIt f;
    f.start_line = j.value("sl", 0); f.start_col = j.value("sc", 0);
    f.end_line   = j.value("el", 0); f.end_col   = j.value("ec", 0);
    f.replacement = j.value("rep", std::string{});
    return f;
}

json diag_to_json(const Diagnostic& d) {
    json fx = json::array();
    for (const auto& f : d.fixits) fx.push_back(fixit_to_json(f));
    return {
        {"rule_id", d.rule_id}, {"rule_name", d.rule_name},
        {"severity", static_cast<int>(d.severity)},
        {"confidence", static_cast<int>(d.confidence)},
        {"file", d.location.file}, {"line", d.location.line}, {"col", d.location.column},
        {"function", d.function_name}, {"message", d.message}, {"fix", d.suggested_fix},
        {"type_size", d.metrics.type_size_bytes}, {"copy_cost", d.metrics.copy_cost_bytes},
        {"lookup", d.metrics.lookup_count}, {"fixits", fx},
    };
}
Diagnostic diag_from_json(const json& j) {
    Diagnostic d;
    d.rule_id = j.value("rule_id", std::string{});
    d.rule_name = j.value("rule_name", std::string{});
    d.severity = static_cast<Severity>(j.value("severity", 2));
    d.confidence = static_cast<Confidence>(j.value("confidence", 1));
    d.location = {j.value("file", std::string{}), j.value("line", 0), j.value("col", 0)};
    d.function_name = j.value("function", std::string{});
    d.message = j.value("message", std::string{});
    d.suggested_fix = j.value("fix", std::string{});
    d.metrics.type_size_bytes = j.value("type_size", 0LL);
    d.metrics.copy_cost_bytes = j.value("copy_cost", 0LL);
    d.metrics.lookup_count = j.value("lookup", 0);
    if (j.contains("fixits"))
        for (const auto& f : j["fixits"]) d.fixits.push_back(fixit_from_json(f));
    return d;
}

json sym_to_json(const SymbolSummary& s) {
    return {
        {"usr", s.usr}, {"qname", s.qualified_name}, {"dname", s.display_name},
        {"file", s.file}, {"line", s.line}, {"def", s.is_definition},
        {"pcount", s.param_count}, {"max_size", s.max_param_size},
        {"max_type", s.max_param_type}, {"max_name", s.max_param_name},
    };
}
SymbolSummary sym_from_json(const json& j) {
    SymbolSummary s;
    s.usr = j.value("usr", std::string{});
    s.qualified_name = j.value("qname", std::string{});
    s.display_name = j.value("dname", std::string{});
    s.file = j.value("file", std::string{});
    s.line = j.value("line", 0);
    s.is_definition = j.value("def", false);
    s.param_count = j.value("pcount", 0);
    s.max_param_size = j.value("max_size", 0LL);
    s.max_param_type = j.value("max_type", std::string{});
    s.max_param_name = j.value("max_name", std::string{});
    return s;
}

}  // namespace

void write_shard(const std::string& path, const ShardArtifact& a) {
    json findings = json::array();
    for (const auto& d : a.findings) findings.push_back(diag_to_json(d));
    json symbols = json::array();
    for (const auto& s : a.symbols) symbols.push_back(sym_to_json(s));
    json edges = json::array();
    for (const auto& [caller, callee] : a.edges) edges.push_back({caller, callee});

    json root = {
        {"schema", "perfguardian-shard-1"},
        {"shard_index", a.shard_index}, {"shard_count", a.shard_count},
        {"findings", findings}, {"symbols", symbols}, {"edges", edges},
    };

    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write shard artifact: " + path);
    out << root.dump(2);
}

ShardArtifact read_shard(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Shard artifact not found: " + path);
    json root;
    try {
        in >> root;
    } catch (const json::exception& e) {
        throw std::runtime_error("Malformed shard artifact " + path + ": " + e.what());
    }

    ShardArtifact a;
    a.shard_index = root.value("shard_index", 0);
    a.shard_count = root.value("shard_count", 1);
    if (root.contains("findings"))
        for (const auto& d : root["findings"]) a.findings.push_back(diag_from_json(d));
    if (root.contains("symbols"))
        for (const auto& s : root["symbols"]) a.symbols.push_back(sym_from_json(s));
    if (root.contains("edges"))
        for (const auto& e : root["edges"])
            a.edges.emplace_back(e.at(0).get<std::string>(), e.at(1).get<std::string>());
    return a;
}

}  // namespace perfguardian
