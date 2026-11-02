#include "flatpak_cli_adapter.hpp"

#include "version_id.hpp"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QSettings>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ecosystem::inventory::flatpak_cli {
namespace {

const QStringList flatpakListArguments{
    QStringLiteral("list"),
    QStringLiteral("--app"),
    QStringLiteral("--columns=application,arch,branch,version,origin,installation,active")
};

std::string trim(const std::string &value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_fields(const std::string &line)
{
    std::vector<std::string> fields;
    // Flatpak list output captured through a pipe is tab-delimited. Non-TSV
    // human table output is intentionally rejected by the caller.
    if (line.find('\t') == std::string::npos)
        return fields;

    std::size_t start = 0;
    for (;;) {
        const auto separator = line.find('\t', start);
        fields.push_back(trim(line.substr(start, separator == std::string::npos
                                                    ? std::string::npos
                                                    : separator - start)));
        if (separator == std::string::npos)
            break;
        start = separator + 1;
    }
    return fields;
}

std::string lower_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool matches_header(const std::vector<std::string> &fields,
                    const std::vector<std::string> &expected)
{
    if (fields.size() < 2 || fields.size() > expected.size())
        return false;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (lower_ascii(fields[i]) != lower_ascii(expected[i]))
            return false;
    }
    return true;
}

bool looks_like_header(const std::vector<std::string> &fields,
                       const std::vector<std::string> &expected)
{
    if (fields.empty())
        return false;
    if (lower_ascii(fields.front()) == lower_ascii(expected.front()))
        return true;
    if (fields.size() < 2 || fields.size() > expected.size())
        return false;
    for (std::size_t i = 1; i < fields.size(); ++i) {
        if (lower_ascii(fields[i]) != lower_ascii(expected[i]))
            return false;
    }
    return true;
}

bool is_commit_id(const std::string &value, std::size_t expected_size)
{
    return value.size() == expected_size
        && std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
}

std::optional<std::string> installation_name_from_cli(const std::string &label)
{
    if (label == "user" || label == "system")
        return label;

    constexpr const char prefix[] = "system (";
    if (label.rfind(prefix, 0) == 0 && label.size() > sizeof(prefix)
        && label.back() == ')') {
        const auto name = label.substr(sizeof(prefix) - 1,
                                       label.size() - sizeof(prefix));
        if (!name.empty())
            return name;
    }
    if (label.find_first_of(" \t\r\n") == std::string::npos)
        return label;
    return std::nullopt;
}

bool is_empty_line(const std::string &line)
{
    return trim(line).empty();
}

ProcessResult run_process(const QString &program, const QStringList &arguments,
                          int timeout_ms, int max_output_bytes)
{
    ProcessResult result;
    if (timeout_ms <= 0 || max_output_bytes <= 0) {
        result.std_err = QStringLiteral("timeout and output limit must be positive");
        return result;
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    // Keep stdout and stderr as independent buffers. MergedChannels would
    // interleave stderr diagnostics into stdout and corrupt TSV parsing.
    process.setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
    environment.insert(QStringLiteral("LANGUAGE"), QStringLiteral("C"));
    process.setProcessEnvironment(environment);

    if (QStandardPaths::findExecutable(program).isEmpty()) {
        result.executable_missing = true;
        return result;
    }

    QElapsedTimer timer;
    timer.start();
    process.start();
    if (!process.waitForStarted(std::min(timeout_ms, 5000))) {
        result.std_err = process.errorString();
        return result;
    }
    result.started = true;

    QByteArray stdoutData;
    QByteArray stderrData;
    bool outputLimitReached = false;
    qsizetype totalOutput = 0;
    const auto appendCapped = [&](QByteArray &destination, const QByteArray &chunk) {
        if (outputLimitReached || chunk.isEmpty())
            return;
        const qsizetype remaining = max_output_bytes - totalOutput;
        if (chunk.size() > remaining) {
            if (remaining > 0) {
                destination.append(chunk.constData(), remaining);
                totalOutput += remaining;
            }
            outputLimitReached = true;
        } else {
            destination.append(chunk);
            totalOutput += chunk.size();
        }
    };
    const auto drainOutput = [&] {
        appendCapped(stdoutData, process.readAllStandardOutput());
        appendCapped(stderrData, process.readAllStandardError());
    };

    while (process.state() != QProcess::NotRunning) {
        drainOutput();
        if (outputLimitReached) {
            process.kill();
            process.waitForFinished(1000);
            drainOutput();
            result.output_truncated = true;
            break;
        }

        const int remainingMs = timeout_ms - static_cast<int>(timer.elapsed());
        if (remainingMs <= 0) {
            process.kill();
            process.waitForFinished(1000);
            drainOutput();
            result.timed_out = true;
            break;
        }
        // Read from either pipe so a full stderr/stdout buffer cannot stall
        // the child while waiting for the other channel.
        process.waitForReadyRead(std::min(remainingMs, 50));
    }
    drainOutput();

    result.exit_code = process.exitCode();
    result.std_out = QString::fromUtf8(stdoutData);
    result.std_err = QString::fromUtf8(stderrData);
    if (process.exitStatus() == QProcess::CrashExit) {
        if (result.std_err.isEmpty())
            result.std_err = QStringLiteral("flatpak process crashed");
    }
    if (outputLimitReached)
        result.output_truncated = true;
    return result;
}

} // namespace

std::optional<std::string> resolve_flatpak_install_root(
    const std::string &installation,
    const QString &home_directory,
    const QString &xdg_data_home,
    const QString &flatpak_user_dir,
    const QString &flatpak_system_dir,
    const QString &system_installations_directory)
{
    const auto absoluteCleanPath = [](QString path) -> std::optional<std::string> {
        path = path.trimmed();
        if (path.isEmpty() || !QDir::isAbsolutePath(path))
            return std::nullopt;
        return QDir::cleanPath(path).toStdString();
    };

    if (installation == "user") {
        if (!flatpak_user_dir.trimmed().isEmpty())
            return absoluteCleanPath(flatpak_user_dir);

        QString dataHome = xdg_data_home.trimmed();
        if (dataHome.isEmpty()) {
            if (home_directory.trimmed().isEmpty())
                return std::nullopt;
            dataHome = QDir(home_directory).filePath(QStringLiteral(".local/share"));
        }
        if (!QDir::isAbsolutePath(dataHome))
            return std::nullopt;
        return absoluteCleanPath(QDir(dataHome).filePath(QStringLiteral("flatpak")));
    }

    if (installation == "system" || installation == "default") {
        const QString root = flatpak_system_dir.trimmed().isEmpty()
            ? QStringLiteral("/var/lib/flatpak") : flatpak_system_dir;
        return absoluteCleanPath(root);
    }

    const QString name = QString::fromStdString(installation);
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")
        || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))
        || system_installations_directory.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const QString group = QStringLiteral("Installation \"%1\"").arg(name);
    const QDir configDirectory(system_installations_directory);
    const QStringList files = configDirectory.entryList(
        {QStringLiteral("*.conf")}, QDir::Files | QDir::Readable, QDir::Name);
    std::optional<std::string> resolvedRoot;
    for (const QString &fileName : files) {
        QSettings settings(configDirectory.filePath(fileName), QSettings::IniFormat);
        if (!settings.childGroups().contains(group))
            continue;
        settings.beginGroup(group);
        const QString path = settings.value(QStringLiteral("Path")).toString();
        settings.endGroup();
        if (settings.status() != QSettings::NoError)
            return std::nullopt;
        const auto candidate = absoluteCleanPath(path);
        if (!candidate || resolvedRoot)
            return std::nullopt;
        resolvedRoot = candidate;
    }
    return resolvedRoot;
}

std::vector<std::string> enumerate_flatpak_system_installations(
    const QString &directory, std::string &error)
{
    error.clear();
    std::vector<std::string> names;
    QDir dir(directory);
    const QFileInfo info(directory);
    if (!info.exists())
        return names;
    if (!info.isDir() || !dir.isReadable()) {
        error = "flatpak system installations directory is not readable: " +
                directory.toStdString();
        return names;
    }

    const QStringList files = dir.entryList(
        {QStringLiteral("*.conf")}, QDir::Files, QDir::Name);
    std::unordered_set<std::string> seen;
    for (const QString &fileName : files) {
        const QFileInfo configFile(dir.filePath(fileName));
        if (!configFile.isReadable()) {
            if (error.empty())
                error = "flatpak system installation config is not readable: " +
                        fileName.toStdString();
            continue;
        }
        QSettings settings(dir.filePath(fileName), QSettings::IniFormat);
        if (settings.status() != QSettings::NoError) {
            if (error.empty())
            error = "flatpak system installation config is malformed: " +
                    fileName.toStdString();
            continue;
        }
        for (const QString &group : settings.childGroups()) {
            const QString prefix = QStringLiteral("Installation \"");
            const QString suffix = QStringLiteral("\"");
            if (!group.startsWith(prefix) || !group.endsWith(suffix) ||
                group.size() <= prefix.size() + suffix.size()) {
                continue;
            }
            const std::string name =
                group.mid(prefix.size(), group.size() - prefix.size() - suffix.size())
                    .toStdString();
            if (name.empty() || !seen.insert(name).second) {
                if (error.empty())
                    error = "duplicate or invalid named flatpak installation: " + name;
                continue;
            }
            names.push_back(name);
        }
    }
    return names;
}

ProcessResult run_flatpak_list(int timeout_ms, int max_output_bytes)
{
    return run_process(QStringLiteral("flatpak"), flatpakListArguments,
                       timeout_ms, max_output_bytes);
}

ProcessResult run_flatpak_process(const QString &program, const QStringList &arguments,
                                 int timeout_ms, int max_output_bytes)
{
    return run_process(program, arguments, timeout_ms, max_output_bytes);
}

ParseResult parse_flatpak_list_output(const QString &output)
{
    ParseResult result;
    // Flatpak emits no header for --columns; an empty successful result is
    // the normal representation of an installation with no applications.
    if (output.isEmpty()) {
        result.ok = true;
        return result;
    }
    std::istringstream stream(output.toStdString());
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    while (!lines.empty() && is_empty_line(lines.back()))
        lines.pop_back();

    if (lines.empty()) {
        result.error = "flatpak output is missing its column header";
        return result;
    }

    std::size_t headerIndex = 0;
    while (headerIndex < lines.size() && is_empty_line(lines[headerIndex]))
        ++headerIndex;
    if (headerIndex == lines.size()) {
        result.error = "flatpak output is missing its column header";
        return result;
    }

    const std::vector<std::string> expected{
        "Application", "Arch", "Branch", "Version", "Origin", "Installation", "Active"
    };
    const auto firstFields = split_fields(lines[headerIndex]);
    if (firstFields.empty()) {
        result.error = "flatpak output is not tab-delimited";
        return result;
    }
    const bool hasHeader = (firstFields.size() == expected.size()
                            || firstFields.size() == expected.size() - 1)
        && matches_header(firstFields, expected);
    if (looks_like_header(firstFields, expected) && !hasHeader) {
        result.error = "malformed flatpak column header";
        return result;
    }
    const std::size_t rowFieldCount = firstFields.size();
    if (!hasHeader && rowFieldCount != expected.size()
        && rowFieldCount != expected.size() - 1) {
        result.error = "unexpected flatpak row column count: "
            + std::to_string(rowFieldCount);
        return result;
    }

    for (std::size_t i = headerIndex + (hasHeader ? 1 : 0); i < lines.size(); ++i) {
        if (is_empty_line(lines[i]))
            continue;
        const auto fields = split_fields(lines[i]);
        if (fields.size() != rowFieldCount) {
            result.error = "row field count mismatch: " + std::to_string(fields.size());
            result.rows.clear();
            return result;
        }

        FlatpakCliRow row;
        row.app_id = fields[0];
        row.architecture = fields[1];
        row.branch = fields[2];
        row.version = fields[3];
        row.origin = fields[4];
        row.installation = fields[5];
        if (fields.size() == expected.size())
            row.active_commit = fields[6];
        if (row.app_id.empty() || row.architecture.empty() || row.branch.empty()
            || row.origin.empty() || row.installation.empty()) {
            result.error = "row has an empty required identity field";
            result.rows.clear();
            return result;
        }
        if (!row.active_commit.empty() && !is_commit_id(row.active_commit, 12)
            && !is_commit_id(row.active_commit, 64)) {
            result.error = "row has an invalid active commit identifier";
            result.rows.clear();
            return result;
        }
        result.rows.push_back(std::move(row));
    }

    result.ok = true;
    return result;
}

UpdateParseResult parse_flatpak_remote_updates_output(const QString &output)
{
    UpdateParseResult result;
    // A successful no-update query is represented by empty stdout, not a
    // header-only table.
    if (output.isEmpty()) {
        result.ok = true;
        return result;
    }
    std::istringstream stream(output.toStdString());
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    while (!lines.empty() && is_empty_line(lines.back()))
        lines.pop_back();
    if (lines.empty()) {
        result.error = "flatpak remote update output is missing its column header";
        return result;
    }

    std::size_t headerIndex = 0;
    while (headerIndex < lines.size() && is_empty_line(lines[headerIndex]))
        ++headerIndex;
    const std::vector<std::string> expected{
        "Application", "Arch", "Branch", "Version", "Commit"
    };
    const auto firstFields = split_fields(lines[headerIndex]);
    if (firstFields.empty()) {
        result.error = "flatpak remote update output is not tab-delimited";
        return result;
    }
    const bool hasHeader = firstFields.size() == expected.size()
        && matches_header(firstFields, expected);
    if (looks_like_header(firstFields, expected) && !hasHeader) {
        result.error = "malformed remote update column header";
        return result;
    }
    if (!hasHeader && firstFields.size() != expected.size()) {
        result.error = "unexpected remote update row column count: "
            + std::to_string(firstFields.size());
        return result;
    }

    for (std::size_t i = headerIndex + (hasHeader ? 1 : 0); i < lines.size(); ++i) {
        if (is_empty_line(lines[i]))
            continue;
        const auto fields = split_fields(lines[i]);
        if (fields.size() != expected.size()) {
            result.error = "remote update row field count mismatch: "
                + std::to_string(fields.size());
            result.rows.clear();
            return result;
        }
        FlatpakUpdateRow row{fields[0], fields[1], fields[2], fields[3], fields[4]};
        if (row.app_id.empty() || row.architecture.empty() || row.branch.empty()
            || (!is_commit_id(row.commit, 12) && !is_commit_id(row.commit, 64))) {
            result.error = "remote update row has an empty required field";
            result.rows.clear();
            return result;
        }
        result.rows.push_back(std::move(row));
    }
    result.ok = true;
    return result;
}

FlatpakCliAdapter::FlatpakCliAdapter(ProcessRunner runner, int timeout_ms,
                                     int max_output_bytes,
                                     InstallRootResolver root_resolver,
                                     QString system_installations_directory)
    : runner_(runner ? std::move(runner) : ProcessRunner(
          [](const QString &program, const QStringList &arguments, int timeout, int outputLimit) {
              return run_process(program, arguments, timeout, outputLimit);
          })),
      root_resolver_(root_resolver ? std::move(root_resolver) : InstallRootResolver(
          [](const std::string &installation) {
              return resolve_flatpak_install_root(
                  installation, QDir::homePath(),
                  qEnvironmentVariable("XDG_DATA_HOME"),
                  qEnvironmentVariable("FLATPAK_USER_DIR"),
                  qEnvironmentVariable("FLATPAK_SYSTEM_DIR"),
                  QStringLiteral("/etc/flatpak/installations.d"));
          })),
      system_installations_directory_(std::move(system_installations_directory)),
      timeout_ms_(std::clamp(timeout_ms, 1, 30000)),
      max_output_bytes_(std::clamp(max_output_bytes, 1, 16 * 1024 * 1024))
{
}

std::string FlatpakCliAdapter::source_id() const
{
    return "flatpak-cli";
}

ecosystem::core::ScanSnapshot FlatpakCliAdapter::discover_instances(
    const ecosystem::core::SnapshotInputs &snapshot_inputs) const
{
    ecosystem::core::ScanSnapshot result;
    result.source_id = source_id();
    result.capture_id = snapshot_inputs.capture_id;
    result.generation = snapshot_inputs.generation;
    result.state = ecosystem::core::SnapshotState::complete;

    const auto addError = [&](ecosystem::core::SourceOperation operation,
                              ecosystem::core::SourceErrorCode code,
                              const std::string &message, bool retryable) {
        result.state = ecosystem::core::SnapshotState::partial;
        result.errors.push_back({result.source_id, operation, code, message, retryable});
    };

    std::unordered_set<std::string> seen_ids_;
    std::unordered_set<std::string> discovered_named_installations;

    const auto processQuery = [&](const QStringList &arguments) {
        const ProcessResult process = runner_(QStringLiteral("flatpak"), arguments,
                                              timeout_ms_, max_output_bytes_);
        if (process.executable_missing) {
            addError(ecosystem::core::SourceOperation::discover_instances,
                     ecosystem::core::SourceErrorCode::manager_unavailable,
                     "flatpak CLI not found on PATH", false);
            return;
        }
        if (!process.started) {
            addError(ecosystem::core::SourceOperation::discover_instances,
                     ecosystem::core::SourceErrorCode::unavailable,
                     "flatpak CLI could not be started", true);
            return;
        }
        if (process.timed_out) {
            addError(ecosystem::core::SourceOperation::discover_instances,
                     ecosystem::core::SourceErrorCode::unavailable,
                     "flatpak CLI timed out", true);
            return;
        }
        if (process.output_truncated) {
            addError(ecosystem::core::SourceOperation::discover_instances,
                     ecosystem::core::SourceErrorCode::malformed_data,
                     "flatpak CLI output exceeded the configured byte limit", false);
            return;
        }
        if (process.exit_code != 0) {
            addError(ecosystem::core::SourceOperation::discover_instances,
                     ecosystem::core::SourceErrorCode::unavailable,
                     "flatpak CLI exited with status " + std::to_string(process.exit_code), true);
            return;
        }

        const ParseResult parsed = parse_flatpak_list_output(process.std_out);
        if (!parsed.ok) {
            addError(ecosystem::core::SourceOperation::parse,
                     ecosystem::core::SourceErrorCode::malformed_data,
                     "flatpak output parse failed: " + parsed.error, false);
            return;
        }

        for (const auto &row : parsed.rows) {
            const auto installationName = installation_name_from_cli(row.installation);
            if (!installationName) {
                addError(ecosystem::core::SourceOperation::parse,
                         ecosystem::core::SourceErrorCode::invalid_identity,
                         "flatpak installation label could not be resolved", false);
                continue;
            }

            const auto installRoot = root_resolver_(*installationName);
            if (!installRoot || installRoot->empty()
                || !QDir::isAbsolutePath(QString::fromStdString(*installRoot))) {
                addError(ecosystem::core::SourceOperation::parse,
                         ecosystem::core::SourceErrorCode::invalid_identity,
                         "flatpak installation root could not be resolved", false);
                continue;
            }

            ecosystem::core::InstanceIdentity identity;
            identity.manager = "flatpak";
            identity.scope = *installationName == "user" ? "user" : "system";
            identity.install_root = *installRoot;
            identity.package_id = row.app_id;
            identity.architecture = row.architecture;
            identity.branch = row.branch;
            identity.origin = row.origin;

            const auto id = ecosystem::core::make_install_instance_id(identity);
            if (!id.id) {
                addError(ecosystem::core::SourceOperation::parse,
                         ecosystem::core::SourceErrorCode::invalid_identity,
                         "flatpak row produced an invalid installation identity", false);
                continue;
            }
            if (!seen_ids_.insert(id.id->value).second) {
                addError(ecosystem::core::SourceOperation::parse,
                         ecosystem::core::SourceErrorCode::duplicate_identity,
                         "duplicate flatpak installation identity: " + id.id->value, false);
                continue;
            }

            std::string activeCommit = row.active_commit;
            if (is_commit_id(activeCommit, 12)) {
                const QString selector = *installationName == "user"
                    ? QStringLiteral("--user")
                    : *installationName == "system"
                        ? QStringLiteral("--system")
                        : QStringLiteral("--installation=%1").arg(
                              QString::fromStdString(*installationName));
                const QStringList infoArguments{
                    QStringLiteral("info"), QStringLiteral("--show-commit"),
                    QStringLiteral("--arch=%1").arg(QString::fromStdString(row.architecture)),
                    selector, QString::fromStdString(row.app_id),
                    QString::fromStdString(row.branch)
                };
                const ProcessResult info = runner_(QStringLiteral("flatpak"), infoArguments,
                                                   timeout_ms_, max_output_bytes_);
                const std::string exactCommit = trim(info.std_out.toStdString());
                if (info.executable_missing) {
                    addError(ecosystem::core::SourceOperation::discover_instances,
                             ecosystem::core::SourceErrorCode::manager_unavailable,
                             "flatpak CLI not found on PATH", false);
                    activeCommit.clear();
                } else if (!info.started || info.timed_out || info.output_truncated
                           || info.exit_code != 0) {
                    addError(ecosystem::core::SourceOperation::discover_instances,
                             info.output_truncated
                                 ? ecosystem::core::SourceErrorCode::malformed_data
                                 : ecosystem::core::SourceErrorCode::unavailable,
                             "flatpak installed commit query failed", !info.output_truncated);
                    activeCommit.clear();
                } else if (!is_commit_id(exactCommit, 64)) {
                    addError(ecosystem::core::SourceOperation::discover_instances,
                             ecosystem::core::SourceErrorCode::malformed_data,
                             "flatpak installed commit query returned an invalid commit", false);
                    activeCommit.clear();
                } else {
                    activeCommit = exactCommit;
                }
            }

            ecosystem::core::InstalledInstance instance;
            instance.software_id.value = row.app_id;
            instance.instance_id = *id.id;
            instance.version.descriptor.manager = "flatpak";
            instance.version.descriptor.raw = row.version;
            instance.version.descriptor.scheme = version_id_001::VersionScheme::opaque;
            instance.trust_source = ecosystem::core::TrustSource::unknown;
            instance.risk = ecosystem::core::UpdateRisk::unknown;
            instance.identity = identity;
            instance.source_revision = std::move(activeCommit);
            result.instances.push_back(std::move(instance));
            if (*installationName != "user" && *installationName != "system")
                discovered_named_installations.insert(*installationName);
        }
    };

    // Retain the existing default user/default system discovery first.
    processQuery(flatpakListArguments);

    // Enumerate configured named system installations read-only. A failure here
    // is reported as partial evidence, but the default query above is retained.
    std::string enumeration_error;
    const auto named_installations = enumerate_flatpak_system_installations(
        system_installations_directory_, enumeration_error);
    if (!enumeration_error.empty()) {
        addError(ecosystem::core::SourceOperation::discover_instances,
                 ecosystem::core::SourceErrorCode::other,
                 enumeration_error, false);
    }

    for (const auto &installation : named_installations) {
        if (discovered_named_installations.contains(installation))
            continue;
        QStringList arguments = flatpakListArguments;
        arguments.push_back(
            QStringLiteral("--installation=%1").arg(QString::fromStdString(installation)));
        processQuery(arguments);
    }

    return result;
}

ecosystem::core::ScanSnapshot FlatpakCliAdapter::discover_candidates(
    const ecosystem::core::SnapshotInputs &snapshot_inputs) const
{
    ecosystem::core::ScanSnapshot result;
    result.source_id = source_id();
    result.capture_id = snapshot_inputs.capture_id;
    result.generation = snapshot_inputs.generation;
    result.state = ecosystem::core::SnapshotState::complete;

    using namespace ecosystem::core;
    using GroupKey = std::tuple<std::string, std::string, std::string, std::string>;
    using RefKey = std::tuple<std::string, std::string, std::string>;
    struct QueryGroup {
        std::vector<const InstalledInstance *> instances;
    };

    const auto checkedAt = QDateTime::currentDateTimeUtc()
        .toString(Qt::ISODateWithMs).toStdString();
    const auto addError = [&](SourceErrorCode code, const std::string &message,
                              bool retryable) {
        result.state = SnapshotState::partial;
        result.errors.push_back({result.source_id, SourceOperation::discover_candidates,
                                 code, message, retryable});
    };
    const auto evidenceFor = [&](const InstalledInstance &instance) {
        UpdateSourceEvidence evidence;
        evidence.source_name = instance.identity.origin;
        evidence.channel = instance.identity.branch;
        evidence.checked_at_utc = checkedAt;
        evidence.freshness = MetadataFreshness::unknown;
        evidence.remote_ref = "app/" + instance.identity.package_id + "/"
            + instance.identity.architecture + "/" + instance.identity.branch;
        evidence.installation_scope = instance.identity.scope;
        evidence.installation_root = instance.identity.install_root;
        // Flatpak does not expose a verifiable source timestamp or cache-hit
        // flag. The ordinary remote query may write summary/index cache data,
        // but does not deploy an application update.
        return evidence;
    };

    std::string installationEnumerationError;
    const auto namedInstallations = enumerate_flatpak_system_installations(
        system_installations_directory_, installationEnumerationError);
    if (!installationEnumerationError.empty())
        addError(SourceErrorCode::other, installationEnumerationError, false);

    const auto selectorFor = [&](const InstalledInstance &instance)
        -> std::optional<QString> {
        if (instance.identity.scope == "user")
            return QStringLiteral("--user");
        if (instance.identity.scope != "system" || instance.identity.install_root.empty())
            return std::nullopt;

        const auto defaultRoot = root_resolver_("system");
        if (defaultRoot && *defaultRoot == instance.identity.install_root)
            return QStringLiteral("--system");

        std::optional<QString> selector;
        for (const auto &name : namedInstallations) {
            const auto root = root_resolver_(name);
            if (!root || *root != instance.identity.install_root)
                continue;
            if (selector)
                return std::nullopt;
            selector = QStringLiteral("--installation=%1").arg(QString::fromStdString(name));
        }
        return selector;
    };

    std::map<GroupKey, QueryGroup> groups;
    for (const auto &instance : snapshot_inputs.instances) {
        if (instance.identity.manager != "flatpak")
            continue;
        if (instance.instance_id.value.empty() || instance.identity.package_id.empty()
            || instance.identity.architecture.empty() || instance.identity.branch.empty()) {
            addError(SourceErrorCode::invalid_identity,
                     "Flatpak candidate check received an incomplete installed identity", false);
            continue;
        }

        const auto selector = selectorFor(instance);
        if (!selector) {
            addError(SourceErrorCode::invalid_identity,
                     "Flatpak installation root does not resolve to one CLI installation", false);
            result.update_checks.push_back({instance.instance_id,
                UpdateCheckStatus::unavailable, evidenceFor(instance)});
            continue;
        }
        if (instance.identity.origin.empty()) {
            addError(SourceErrorCode::invalid_identity,
                     "Flatpak installed ref has no recorded origin remote", false);
            result.update_checks.push_back({instance.instance_id,
                UpdateCheckStatus::unavailable, evidenceFor(instance)});
            continue;
        }

        groups[{instance.identity.scope, instance.identity.install_root,
                instance.identity.origin, selector->toStdString()}]
            .instances.push_back(&instance);
    }

    for (const auto &[key, group] : groups) {
        const auto &[scope, root, origin, selector] = key;
        Q_UNUSED(scope)
        Q_UNUSED(root)

        QStringList arguments{
            QStringLiteral("remote-ls"),
            QStringLiteral("--updates"),
            QStringLiteral("--app"),
            QStringLiteral("--columns=application,arch,branch,version,commit"),
            QString::fromStdString(selector),
            QString::fromStdString(origin)
        };
        const ProcessResult process = runner_(QStringLiteral("flatpak"), arguments,
                                              timeout_ms_, max_output_bytes_);
        if (process.executable_missing) {
            addError(SourceErrorCode::manager_unavailable,
                     "flatpak CLI not found on PATH", false);
            for (const auto *instance : group.instances)
                result.update_checks.push_back({instance->instance_id,
                    UpdateCheckStatus::unavailable, evidenceFor(*instance)});
            continue;
        }
        if (!process.started || process.timed_out || process.output_truncated
            || process.exit_code != 0) {
            const std::string message = !process.started
                ? "flatpak remote update query could not be started"
                : process.timed_out
                    ? "flatpak remote update query timed out"
                    : process.output_truncated
                        ? "flatpak remote update output exceeded the configured byte limit"
                        : "flatpak remote update query exited with status "
                            + std::to_string(process.exit_code);
            addError(process.output_truncated ? SourceErrorCode::malformed_data
                                              : SourceErrorCode::unavailable,
                     message, !process.output_truncated);
            for (const auto *instance : group.instances)
                result.update_checks.push_back({instance->instance_id,
                    UpdateCheckStatus::unavailable, evidenceFor(*instance)});
            continue;
        }

        const UpdateParseResult parsed = parse_flatpak_remote_updates_output(process.std_out);
        if (!parsed.ok) {
            addError(SourceErrorCode::malformed_data,
                     "flatpak remote update output parse failed: " + parsed.error, false);
            for (const auto *instance : group.instances)
                result.update_checks.push_back({instance->instance_id,
                    UpdateCheckStatus::unavailable, evidenceFor(*instance)});
            continue;
        }

        std::map<RefKey, const InstalledInstance *> installedByRef;
        std::unordered_set<std::string> ambiguousIds;
        for (const auto *instance : group.instances) {
            const RefKey refKey{instance->identity.package_id,
                                instance->identity.architecture,
                                instance->identity.branch};
            const auto [it, inserted] = installedByRef.emplace(refKey, instance);
            if (!inserted) {
                ambiguousIds.insert(it->second->instance_id.value);
                ambiguousIds.insert(instance->instance_id.value);
            }
        }

        std::unordered_set<std::string> candidateIds;
        std::unordered_set<std::string> seenUpdateRefs;
        for (const auto &row : parsed.rows) {
            const RefKey refKey{row.app_id, row.architecture, row.branch};
            const auto installed = installedByRef.find(refKey);
            if (installed == installedByRef.end()) {
                addError(SourceErrorCode::unmatched_candidate,
                         "Flatpak remote update does not match an installed ref in this snapshot",
                         false);
                continue;
            }
            const InstalledInstance &instance = *installed->second;
            if (ambiguousIds.contains(instance.instance_id.value)) {
                addError(SourceErrorCode::duplicate_identity,
                         "Flatpak remote update matches multiple installed refs", false);
                continue;
            }
            if (!seenUpdateRefs.insert(instance.instance_id.value).second) {
                addError(SourceErrorCode::duplicate_identity,
                         "Flatpak remote query returned a duplicate update ref", false);
                continue;
            }
            if (instance.source_revision.empty()) {
                addError(SourceErrorCode::inconsistent_snapshot,
                         "installed Flatpak ref has no active commit for update comparison", false);
                result.update_checks.push_back({instance.instance_id,
                    UpdateCheckStatus::unavailable, evidenceFor(instance)});
                continue;
            }

            UpdateSourceEvidence evidence = evidenceFor(instance);
            std::string candidateCommit = row.commit;
            if (is_commit_id(candidateCommit, 12)) {
                const QString ref = QStringLiteral("app/%1/%2/%3")
                    .arg(QString::fromStdString(instance.identity.package_id),
                         QString::fromStdString(instance.identity.architecture),
                         QString::fromStdString(instance.identity.branch));
                const QStringList infoArguments{
                    QStringLiteral("remote-info"), QStringLiteral("--show-commit"),
                    QString::fromStdString(selector),
                    QString::fromStdString(origin), ref
                };
                const ProcessResult info = runner_(QStringLiteral("flatpak"), infoArguments,
                                                   timeout_ms_, max_output_bytes_);
                candidateCommit = trim(info.std_out.toStdString());
                if (info.executable_missing || !info.started || info.timed_out
                    || info.output_truncated || info.exit_code != 0
                    || !is_commit_id(candidateCommit, 64)) {
                    const bool truncated = info.output_truncated;
                    addError(info.executable_missing
                                 ? SourceErrorCode::manager_unavailable
                                 : truncated ? SourceErrorCode::malformed_data
                                             : SourceErrorCode::unavailable,
                             info.executable_missing
                                 ? "flatpak CLI not found on PATH"
                                 : "flatpak remote commit query failed",
                             !info.executable_missing && !truncated);
                    result.update_checks.push_back({instance.instance_id,
                        UpdateCheckStatus::unavailable, std::move(evidence)});
                    continue;
                }
            }
            evidence.metadata_revision = candidateCommit;
            if (candidateCommit == instance.source_revision) {
                result.update_checks.push_back({instance.instance_id,
                    UpdateCheckStatus::observed_revision_matches, std::move(evidence)});
                continue;
            }

            UpdateCandidate candidate;
            candidate.software_id = instance.software_id;
            candidate.instance_id = instance.instance_id;
            candidate.version.descriptor.manager = "flatpak";
            candidate.version.descriptor.raw = row.version;
            candidate.version.descriptor.scheme = version_id_001::VersionScheme::opaque;
            candidate.trust_source = TrustSource::unknown;
            candidate.risk = UpdateRisk::unknown;
            candidate.source_evidence = std::move(evidence);
            candidateIds.insert(instance.instance_id.value);
            result.candidates.push_back(std::move(candidate));
        }

        for (const auto *instance : group.instances) {
            if (ambiguousIds.contains(instance->instance_id.value)) {
                result.update_checks.push_back({instance->instance_id,
                    UpdateCheckStatus::unavailable, evidenceFor(*instance)});
                continue;
            }
            if (!candidateIds.contains(instance->instance_id.value)
                && !seenUpdateRefs.contains(instance->instance_id.value)) {
                result.update_checks.push_back({instance->instance_id,
                    UpdateCheckStatus::no_candidate, evidenceFor(*instance)});
            }
        }
    }

    return result;
}

} // namespace ecosystem::inventory::flatpak_cli
