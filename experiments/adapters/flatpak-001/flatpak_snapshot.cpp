#include "flatpak_snapshot.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace ecosystem::flatpak {
namespace {

bool valid_installation(const std::string& value) {
    if (value == "user" || value == "system") return true;
    constexpr const char* prefix = "installation:";
    if (value.compare(0, 13, prefix) != 0 || value.size() == 13) return false;
    return std::all_of(value.begin() + 13, value.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}

bool bad_control(const std::string& field) {
    for (unsigned char c : field) {
        // Disallow embedded controls and ANSI escape characters; tabs are
        // separators and must not remain within a parsed field.
        if (c < 0x20 || c == 0x7f) return true;
    }
    return false;
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t pos = 0;
    for (;;) {
        auto next = line.find('\t', pos);
        fields.emplace_back(line.substr(pos, next == std::string::npos
                                                   ? std::string::npos : next - pos));
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    return fields;
}

void add_component(std::string& result, const std::string& value) {
    result += std::to_string(value.size());
    result += ':';
    result += value;
    result += '|';
}

}  // namespace

ParseResult parse_tsv(const std::string& text, const std::string& installation,
                      bool has_header) {
    ParseResult result;
    if (!valid_installation(installation)) {
        result.issues.push_back({0, "invalid installation scope"});
        return result;
    }
    std::size_t offset = 0;
    std::size_t line_no = 0;
    while (offset < text.size()) {
        const auto end = text.find('\n', offset);
        std::string line = text.substr(offset, end == std::string::npos
                                               ? std::string::npos : end - offset);
        ++line_no;
        offset = end == std::string::npos ? text.size() : end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (has_header && line_no == 1) continue;
        if (line.empty()) continue;
        const auto fields = split_tabs(line);
        if (fields.size() != 5) {
            result.issues.push_back({line_no, "expected exactly five tab-separated fields"});
            continue;
        }
        if (std::any_of(fields.begin(), fields.end(), bad_control)) {
            result.issues.push_back({line_no, "control character in field"});
            continue;
        }
        // Version is allowed to be empty: Flatpak may expose no version even
        // when a remote update candidate exists. Preserve that uncertainty.
        if (fields[0].empty() || fields[1].empty() || fields[2].empty()
            || fields[4].empty()) {
            result.issues.push_back({line_no, "required identity field is empty"});
            continue;
        }
        result.rows.push_back({fields[0], fields[1], fields[2], fields[3],
                               fields[4], installation});
    }
    return result;
}

std::string identity_key(const Row& row) {
    // Length-prefix every component so different dimensions cannot collide.
    std::string result;
    add_component(result, row.installation);
    add_component(result, row.application);
    add_component(result, row.arch);
    add_component(result, row.branch);
    add_component(result, row.origin);
    return result;
}

ResolveResult reconcile(const ParseResult& installed, const ParseResult& updates) {
    ResolveResult result;
    result.issues.insert(result.issues.end(), installed.issues.begin(), installed.issues.end());
    result.issues.insert(result.issues.end(), updates.issues.begin(), updates.issues.end());
    std::unordered_map<std::string, std::size_t> positions;
    for (const auto& row : installed.rows) {
        const auto key = identity_key(row);
        if (positions.find(key) != positions.end()) {
            result.issues.push_back({0, "duplicate installed instance identity"});
            continue;
        }
        positions.emplace(key, result.rows.size());
        result.rows.push_back({row, UpdateStatus::Unknown, {}});
    }
    std::unordered_set<std::string> seen_updates;
    for (const auto& candidate : updates.rows) {
        const auto key = identity_key(candidate);
        if (!seen_updates.insert(key).second) {
            result.issues.push_back({0, "duplicate update candidate identity"});
            continue;
        }
        const auto it = positions.find(key);
        if (it == positions.end()) {
            result.issues.push_back({0, "unmatched update candidate identity"});
            continue;
        }
        auto& entry = result.rows[it->second];
        entry.status = UpdateStatus::Candidate;
        entry.candidate_version = candidate.version;
    }
    return result;
}

}  // namespace ecosystem::flatpak
