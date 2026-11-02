#pragma once

#include "core_model.hpp"

#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ecosystem::inventory::flatpak_cli {

// Result of invoking the Flatpak CLI. Kept separate from parsing so that
// process execution can be replaced in tests without touching the parser.
struct ProcessResult {
    bool started = false;
    bool timed_out = false;
    bool output_truncated = false;
    int exit_code = -1;
    QString std_out;
    QString std_err;
    bool executable_missing = false;
};

// Injectable process runner. The default implementation uses QProcess with a
// strict timeout and an output cap; tests inject canned results.
using ProcessRunner = std::function<ProcessResult(const QString &program,
                                                  const QStringList &arguments,
                                                  int timeout_ms,
                                                  int max_output_bytes)>;

// Resolves the CLI's installation identifier to the installation's actual
// absolute data root. An unresolved name must not be treated as a path.
using InstallRootResolver = std::function<std::optional<std::string>(const std::string &installation)>;

// Enumerates configured named system installations from installations.d.
// Returns their installation names; error is non-empty on enumeration failure.
std::vector<std::string> enumerate_flatpak_system_installations(
    const QString &directory,
    std::string &error);

// One parsed Flatpak installation record. All fields are preserved verbatim
// from the CLI output; no version interpretation or candidate inference.
struct FlatpakCliRow {
    std::string app_id;
    std::string architecture;
    std::string branch;
    std::string version;        // opaque descriptor, never compared
    std::string origin;
    std::string installation;   // installation name/root identifier
    std::string active_commit;
};

struct ParseResult {
    std::vector<FlatpakCliRow> rows;
    bool ok = false;
    std::string error;
};

struct FlatpakUpdateRow {
    std::string app_id;
    std::string architecture;
    std::string branch;
    std::string version;
    std::string commit;
};

struct UpdateParseResult {
    std::vector<FlatpakUpdateRow> rows;
    bool ok = false;
    std::string error;
};

// Pure, separately testable parser. Validates the header and every row; fails
// closed (ok=false) on missing/extra/ambiguous/truncated records.
ParseResult parse_flatpak_list_output(const QString &output);
UpdateParseResult parse_flatpak_remote_updates_output(const QString &output);

std::optional<std::string> resolve_flatpak_install_root(
    const std::string &installation,
    const QString &home_directory,
    const QString &xdg_data_home,
    const QString &flatpak_user_dir,
    const QString &flatpak_system_dir,
    const QString &system_installations_directory);

// Default QProcess-backed runner. Bounded by timeout_ms and max_output_bytes.
ProcessResult run_flatpak_list(int timeout_ms, int max_output_bytes);

// Injectable QProcess-backed execution helper used by tests and the default runner.
ProcessResult run_flatpak_process(const QString &program, const QStringList &arguments,
                                 int timeout_ms, int max_output_bytes);

class FlatpakCliAdapter : public ecosystem::core::SourceAdapter {
public:
    explicit FlatpakCliAdapter(ProcessRunner runner = {},
                               int timeout_ms = 10000,
                               int max_output_bytes = 1024 * 1024,
                               InstallRootResolver root_resolver = {},
                               QString system_installations_directory =
                                   QStringLiteral("/etc/flatpak/installations.d"));

    std::string source_id() const override;
    ecosystem::core::ScanSnapshot discover_instances(
        const ecosystem::core::SnapshotInputs &snapshot_inputs) const override;
    ecosystem::core::ScanSnapshot discover_candidates(
        const ecosystem::core::SnapshotInputs &snapshot_inputs) const override;

    // Exposed for tests: parse a captured stdout blob.
    static ParseResult parse(const QString &output) { return parse_flatpak_list_output(output); }
    static UpdateParseResult parse_updates(const QString &output)
    {
        return parse_flatpak_remote_updates_output(output);
    }

private:
    ProcessRunner runner_;
    InstallRootResolver root_resolver_;
    QString system_installations_directory_;
    int timeout_ms_;
    int max_output_bytes_;
};

} // namespace ecosystem::inventory::flatpak_cli
