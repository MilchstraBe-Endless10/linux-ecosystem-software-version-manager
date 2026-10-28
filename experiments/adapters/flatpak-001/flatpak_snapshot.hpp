#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ecosystem::flatpak {

// Parsing only. No subprocesses, networking, filesystem reads, or update execution.
struct Row {
    std::string application;
    std::string arch;
    std::string branch;
    std::string version;
    std::string origin;
    std::string installation;
};

struct Issue {
    std::size_t line = 0;
    std::string message;
};

struct ParseResult {
    std::vector<Row> rows;
    std::vector<Issue> issues;
};

// The caller supplies the installation context separately, because remote-ls
// does not have an 'installation' column. E.g. "user", "system", "installation:extra".
// Input is five TAB-separated columns (application,arch,branch,version,origin).
// The CLI's localized header is skipped by physical position, not by matching text.
ParseResult parse_tsv(const std::string& text,
                      const std::string& installation,
                      bool has_header = true);

std::string identity_key(const Row& row);

enum class UpdateStatus {
    Unknown,       // No matching candidate; not a claim that the app is current.
    Candidate       // Explicitly present in a separately supplied updates snapshot.
};

struct Resolved {
    Row installed;
    UpdateStatus status = UpdateStatus::Unknown;
    std::string candidate_version;
};

struct ResolveResult {
    std::vector<Resolved> rows;
    std::vector<Issue> issues;
};

// Does not compare version strings or infer update eligibility from versions.
// Both inputs must be captured from the SAME installation context and scope.
ResolveResult reconcile(const ParseResult& installed,
                        const ParseResult& updates);

}  // namespace ecosystem::flatpak
