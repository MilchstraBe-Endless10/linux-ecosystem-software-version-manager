#include "flatpak_cli_adapter.hpp"
#include "core_model.hpp"
#include "version_id.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace ecosystem::core;
using namespace ecosystem::inventory::flatpak_cli;

namespace {
int failures = 0;
void check(bool cond, const std::string &msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

InstalledInstance make_flatpak_instance(const std::string &version,
                                        const std::string &commit)
{
    InstanceIdentity identity;
    identity.manager = "flatpak";
    identity.scope = "user";
    identity.install_root = "/home/tester/.local/share/flatpak";
    identity.package_id = "org.example.App";
    identity.architecture = "x86_64";
    identity.branch = "stable";
    identity.origin = "flathub";
    const auto id = make_install_instance_id(identity);

    InstalledInstance instance;
    instance.software_id = {identity.package_id};
    instance.instance_id = id.id.value_or(InstallInstanceId{});
    instance.version.descriptor.manager = "flatpak";
    instance.version.descriptor.raw = version;
    instance.version.descriptor.scheme = version_id_001::VersionScheme::opaque;
    instance.identity = identity;
    instance.source_revision = commit;
    return instance;
}
} // namespace

// Restores PATH and the synthetic-mode marker on every exit path.
struct EnvironmentGuard {
    const QByteArray path;
    const QByteArray syntheticMode;
    bool active = true;
    EnvironmentGuard()
        : path(qgetenv("PATH")),
          syntheticMode(qgetenv("FLATPAK_SYNTHETIC_MODE")) {}
    ~EnvironmentGuard() {
        if (active) {
            qputenv("PATH", path);
            if (syntheticMode.isEmpty()) {
                qunsetenv("FLATPAK_SYNTHETIC_MODE");
            } else {
                qputenv("FLATPAK_SYNTHETIC_MODE", syntheticMode);
            }
        }
    }
    void release() { active = false; }
};

int main() {
    // --- Installation identifier resolves to an absolute data root ---
    {
        QTemporaryDir temp;
        check(temp.isValid(), "temporary directory available for root fixtures");
        const QString configDirectory = QDir(temp.path()).filePath("installations.d");
        check(QDir().mkpath(configDirectory), "custom installation config directory created");

        const auto defaultUser = resolve_flatpak_install_root(
            "user", "/home/tester", "", "", "", configDirectory);
        check(defaultUser == std::optional<std::string>("/home/tester/.local/share/flatpak"),
              "user root follows home default");
        const auto xdgUser = resolve_flatpak_install_root(
            "user", "/home/tester", "/data/tester", "", "", configDirectory);
        check(xdgUser == std::optional<std::string>("/data/tester/flatpak"),
              "user root follows XDG_DATA_HOME");
        const auto explicitUser = resolve_flatpak_install_root(
            "user", "/home/tester", "/data/tester", "/custom/user-flatpak", "", configDirectory);
        check(explicitUser == std::optional<std::string>("/custom/user-flatpak"),
              "FLATPAK_USER_DIR takes precedence");
        const auto defaultSystem = resolve_flatpak_install_root(
            "system", "/home/tester", "", "", "", configDirectory);
        check(defaultSystem == std::optional<std::string>("/var/lib/flatpak"),
              "default system root");
        const auto explicitSystem = resolve_flatpak_install_root(
            "default", "/home/tester", "", "", "/custom/system-flatpak", configDirectory);
        check(explicitSystem == std::optional<std::string>("/custom/system-flatpak"),
              "FLATPAK_SYSTEM_DIR takes precedence");

        const QString customConfig = QDir(configDirectory).filePath("custom.conf");
        QFile file(customConfig);
        check(file.open(QIODevice::WriteOnly | QIODevice::Text), "custom installation fixture opened");
        file.write(R"([Installation "workstation"]
Path=/mnt/flatpak-workstation
)");
        file.close();
        const auto customSystem = resolve_flatpak_install_root(
            "workstation", "/home/tester", "", "", "", configDirectory);
        check(customSystem == std::optional<std::string>("/mnt/flatpak-workstation"),
              "custom system root read from its exact config");
        check(!resolve_flatpak_install_root(
                  "../outside", "/home/tester", "", "", "", configDirectory),
              "installation path traversal rejected");
        check(!resolve_flatpak_install_root(
                  "missing", "/home/tester", "", "", "", configDirectory),
              "unknown custom installation stays unresolved");
    }

    // --- Parser: valid output ---
    {
        const QString output =
            "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\tActive\n"
            "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\t"
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
            "org.example.App\tx86_64\tstable\t1.0\tflathub\tsystem\t"
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
        auto parsed = parse_flatpak_list_output(output);
        check(parsed.ok, "valid parse ok");
        check(parsed.rows.size() == 2, "valid row count");
        if (parsed.rows.size() == 2) {
            check(parsed.rows[0].app_id == "org.example.App", "app id 0");
            check(parsed.rows[0].architecture == "x86_64", "arch 0");
            check(parsed.rows[0].branch == "stable", "branch 0");
            check(parsed.rows[0].version == "1.0", "version 0");
            check(parsed.rows[0].origin == "flathub", "origin 0");
            check(parsed.rows[0].installation == "user", "installation 0");
            check(parsed.rows[0].active_commit
                      == "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
                  "active revision 0");
            check(parsed.rows[1].installation == "system", "installation 1");
        }
    }

    // Flatpak 1.18 --columns output is headerless TSV and may abbreviate commits.
    {
        const QString output =
            "org.example.App\tx86_64\tstable\t\tflathub\tuser\t"
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
        const auto parsed = parse_flatpak_list_output(output);
        check(parsed.ok && parsed.rows.size() == 1,
              "headerless installed output parses as a row");
        if (parsed.rows.size() == 1) {
            check(parsed.rows[0].version.empty(),
                  "headerless installed output preserves an absent AppStream version");
            check(parsed.rows[0].active_commit.size() == 64,
                  "full active commit remains intact");
        }
    }

    // --- Parser: remote update rows preserve exact ref identity and revision ---
    {
        const auto parsed = parse_flatpak_remote_updates_output(
            "Application\tArch\tBranch\tVersion\tCommit\n"
            "org.example.App\tx86_64\tstable\t1.0\t"
            "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\n");
        check(parsed.ok, "remote update output parses");
        check(parsed.rows.size() == 1, "remote update row count");
        if (parsed.rows.size() == 1) {
            check(parsed.rows[0].app_id == "org.example.App", "remote update app ID retained");
            check(parsed.rows[0].architecture == "x86_64", "remote update architecture retained");
            check(parsed.rows[0].branch == "stable", "remote update branch retained");
            check(parsed.rows[0].version == "1.0", "remote update version retained opaque");
            check(parsed.rows[0].commit
                      == "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210",
                  "remote update commit retained");
        }
        const auto malformed = parse_flatpak_remote_updates_output(
            "Application\tArch\tBranch\tVersion\tCommit\n"
            "org.example.App\tx86_64\tstable\t1.0\n");
        check(!malformed.ok && malformed.rows.empty(),
              "incomplete remote update row fails closed");

        const auto cliOutput = parse_flatpak_remote_updates_output(
            "org.example.App\tx86_64\tstable\t\t"
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n");
        check(cliOutput.ok && cliOutput.rows.size() == 1,
              "headerless real CLI update output parses");
        if (cliOutput.rows.size() == 1) {
            check(cliOutput.rows.front().version.empty(),
                  "real CLI update may omit AppStream version");
            check(cliOutput.rows.front().commit.size() == 64,
                  "real CLI update preserves a full commit");
        }
    }

    // --- Parser: successful empty CLI output means no installed refs ---
    {
        auto parsed = parse_flatpak_list_output(QString(""));
        check(parsed.ok, "empty installed output is a valid empty snapshot");
        check(parsed.rows.empty(), "empty rows");
    }

    // --- Parser: successful empty remote output means no candidate rows ---
    {
        const auto parsed = parse_flatpak_remote_updates_output(QString(""));
        check(parsed.ok && parsed.rows.empty(),
              "empty update output is a valid no-candidate result");
    }

    // --- Parser: whitespace-only output fails closed (no required header) ---
    {
        auto parsed = parse_flatpak_list_output(QString("   \n\n"));
        check(!parsed.ok, "whitespace-only fails");
        check(parsed.rows.empty(), "whitespace rows empty");
    }

    // --- Parser: valid header with no rows accepted as empty inventory ---
    {
        const QString output = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n";
        auto parsed = parse_flatpak_list_output(output);
        check(parsed.ok, "header-only ok");
        check(parsed.rows.empty(), "header-only empty rows");
    }

    // --- Parser: wrong header count ---
    {
        const QString output = "Application\tArch\tBranch\norg.example.App\tx86_64\tstable\n";
        auto parsed = parse_flatpak_list_output(output);
        check(!parsed.ok, "wrong header count fails");
    }

    // --- Parser: bad header name ---
    {
        const QString output = "Foo\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n";
        auto parsed = parse_flatpak_list_output(output);
        check(!parsed.ok, "bad header name fails");
    }

    // --- Parser: row field count mismatch ---
    {
        const QString output = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.App\tx86_64\tstable\n";
        auto parsed = parse_flatpak_list_output(output);
        check(!parsed.ok, "row field mismatch fails");
    }

    // --- Parser: empty required field ---
    {
        const QString output = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "\tx86_64\tstable\t1.0\tflathub\tuser\n";
        auto parsed = parse_flatpak_list_output(output);
        check(!parsed.ok, "empty app_id fails");
    }
    {
        const QString output = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.App\t\tstable\t1.0\tflathub\tuser\n";
        auto parsed = parse_flatpak_list_output(output);
        check(!parsed.ok, "empty arch fails");
    }
    {
        const QString output = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.App\tx86_64\tstable\t1.0\tflathub\t\n";
        auto parsed = parse_flatpak_list_output(output);
        check(!parsed.ok, "empty installation fails");
    }

    // --- Parser: trailing empty lines tolerated ---
    {
        const QString output = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n"
                               "\n\n";
        auto parsed = parse_flatpak_list_output(output);
        check(parsed.ok, "trailing blanks ok");
        check(parsed.rows.size() == 1, "trailing blanks row count");
    }

    // --- Adapter: unavailable runner ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = false;
            r.std_err = "not found";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-u";
        inputs.generation = 1;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::partial, "unavailable state partial");
        check(snap.instances.empty(), "unavailable no instances");
        check(!snap.errors.empty(), "unavailable has error");
        check(snap.errors[0].code == SourceErrorCode::unavailable, "unavailable code");
    }

    // --- Adapter: timeout ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = true;
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-t";
        inputs.generation = 2;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::partial, "timeout state partial");
        check(snap.errors[0].code == SourceErrorCode::unavailable, "timeout code");
    }

    // --- Adapter: output truncated ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.output_truncated = true;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                        "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-o";
        inputs.generation = 3;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::partial, "truncated state partial");
        check(snap.errors[0].code == SourceErrorCode::malformed_data, "truncated code");
    }

    // --- Adapter: nonzero exit ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 1;
            r.std_err = "https://user:secret@example.invalid TOKEN_SENTINEL";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-e";
        inputs.generation = 4;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::partial, "exit state partial");
        check(snap.errors[0].code == SourceErrorCode::unavailable, "exit code");
        check(snap.errors[0].message.find("TOKEN_SENTINEL") == std::string::npos,
              "process diagnostic does not leak stderr secrets");
        check(snap.errors[0].message.find("user:secret") == std::string::npos,
              "process diagnostic does not leak remote credentials");
    }

    // --- Adapter: parse failure via runner ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.std_out = "not a valid header\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-p";
        inputs.generation = 5;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::partial, "parse state partial");
        check(snap.errors[0].code == SourceErrorCode::malformed_data, "parse code");
    }

    // --- Adapter: successful discovery preserves identity ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                        "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n"
                        "org.example.App\tx86_64\tstable\t1.0\tflathub\tsystem\n";
            return r;
        };
        InstallRootResolver roots = [](const std::string &installation) -> std::optional<std::string> {
            if (installation == "user") return "/home/tester/.local/share/flatpak";
            if (installation == "system") return "/var/lib/flatpak";
            return std::nullopt;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024, roots);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-ok";
        inputs.generation = 6;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::complete, "ok state complete");
        check(snap.source_id == "flatpak-cli", "ok source id");
        check(snap.capture_id == "cap-ok", "ok capture id");
        check(snap.generation == 6, "ok generation");
        check(snap.instances.size() == 2, "ok instance count");
        check(snap.errors.empty(), "ok no errors");
        if (snap.instances.size() == 2) {
            check(snap.instances[0].software_id.value == "org.example.App", "software id");
            check(snap.instances[0].instance_id.value != snap.instances[1].instance_id.value,
                  "user/system distinct");
            check(snap.instances[0].version.descriptor.raw == "1.0", "version preserved");
            check(snap.instances[0].version.descriptor.scheme == version_id_001::VersionScheme::opaque,
                  "version opaque");
            check(snap.instances[0].identity.install_root == "/home/tester/.local/share/flatpak",
                  "user installation root preserved");
            check(snap.instances[1].identity.install_root == "/var/lib/flatpak",
                  "system installation root preserved");
        }
    }

    // --- Parser: TSV row with empty optional version is preserved ---
    {
        const QString output =
            "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
            "org.example.App\tx86_64\tstable\t\tflathub\tuser\n";
        auto parsed = parse_flatpak_list_output(output);
        check(parsed.ok, "empty version tsv ok");
        check(parsed.rows.size() == 1, "empty version tsv row count");
        if (parsed.rows.size() == 1) {
            check(parsed.rows[0].app_id == "org.example.App", "empty version app id");
            check(parsed.rows[0].version.empty(), "empty version preserved");
        }
    }

    // --- Adapter: empty optional version survives the installed-instance projection ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                        "org.example.App\tx86_64\tstable\t\tflathub\tuser\n";
            return r;
        };
        InstallRootResolver roots = [](const std::string &installation) -> std::optional<std::string> {
            return installation == "user"
                ? std::optional<std::string>("/home/tester/.local/share/flatpak")
                : std::nullopt;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024, roots);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-empty-version";
        inputs.generation = 11;

        const auto installed = adapter.discover_instances(inputs);
        check(installed.state == SnapshotState::complete,
              "empty optional version keeps installed scan complete");
        check(installed.instances.size() == 1,
              "empty optional version keeps the installed instance");
        if (installed.instances.size() == 1) {
            const auto &instance = installed.instances.front();
            check(!instance.instance_id.value.empty(), "empty version identity remains valid");
            check(instance.identity.manager == "flatpak", "empty version manager preserved");
            check(instance.identity.scope == "user", "empty version scope preserved");
            check(instance.identity.install_root == "/home/tester/.local/share/flatpak",
                  "empty version installation root preserved");
            check(instance.identity.package_id == "org.example.App",
                  "empty version package identity preserved");
            check(instance.identity.architecture == "x86_64",
                  "empty version architecture preserved");
            check(instance.identity.branch == "stable", "empty version branch preserved");
            check(instance.identity.origin == "flathub", "empty version origin preserved");
            check(instance.version.descriptor.raw.empty(), "empty opaque version preserved");
            check(instance.version.descriptor.scheme == version_id_001::VersionScheme::opaque,
                  "empty version remains opaque");
        }

        const auto candidates = adapter.discover_candidates(inputs);
        check(candidates.state == SnapshotState::complete,
              "empty version does not make candidate discovery incomplete");
        check(candidates.candidates.empty(),
              "empty installed version does not infer an update candidate");
    }

    // --- Adapter: CLI's abbreviated installed commit is resolved exactly ---
    {
        const std::string installedCommit =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        std::vector<QStringList> calls;
        ProcessRunner runner = [&](const QString &, const QStringList &arguments, int, int) {
            calls.push_back(arguments);
            ProcessResult r;
            r.started = true;
            r.exit_code = 0;
            if (arguments.value(0) == QStringLiteral("list")) {
                r.std_out = "org.example.App\tx86_64\tstable\t\tflathub\tuser\t0123456789ab\n";
            } else if (arguments.value(0) == QStringLiteral("info")) {
                r.std_out = QString::fromStdString(installedCommit) + QLatin1Char('\n');
            } else {
                r.exit_code = 9;
            }
            return r;
        };
        InstallRootResolver roots = [](const std::string &installation)
            -> std::optional<std::string> {
            return installation == "user"
                ? std::optional<std::string>("/home/tester/.local/share/flatpak")
                : std::nullopt;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024, roots,
                                  QStringLiteral("/definitely/missing/installations.d"));
        const auto snapshot = adapter.discover_instances({});
        check(calls.size() == 2, "abbreviated installed commit triggers one exact info query");
        if (calls.size() == 2) {
            check(calls[1] == QStringList{
                      QStringLiteral("info"), QStringLiteral("--show-commit"),
                      QStringLiteral("--arch=x86_64"), QStringLiteral("--user"),
                      QStringLiteral("org.example.App"), QStringLiteral("stable")},
                  "installed commit query pins exact scope, architecture, app, and branch");
        }
        check(snapshot.state == SnapshotState::complete && snapshot.instances.size() == 1,
              "real CLI installation identity remains complete");
        if (snapshot.instances.size() == 1)
            check(snapshot.instances.front().source_revision == installedCommit,
                  "installed source revision is the full 64-character commit");
    }

    // --- Parser: non-TSV human table output is rejected ---
    {
        const QString output =
            "Application Arch Branch Version Origin Installation\n"
            "org.example.App x86_64 stable 1.0 flathub user\n";
        auto parsed = parse_flatpak_list_output(output);
        check(!parsed.ok, "non-tsv table rejected");
        check(parsed.rows.empty(), "non-tsv rows empty");
    }

    // --- Adapter: named system installation resolves its configured root ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                        "org.example.App\tx86_64\tstable\t1.0\tflathub\tworkstation\n";
            return r;
        };
        InstallRootResolver roots = [](const std::string &installation) -> std::optional<std::string> {
            return installation == "workstation"
                ? std::optional<std::string>("/mnt/flatpak-workstation")
                : std::nullopt;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024, roots);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-named";
        inputs.generation = 10;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::complete, "named installation complete");
        check(snap.instances.size() == 1, "named installation count");
        if (snap.instances.size() == 1) {
            check(snap.instances[0].identity.scope == "system", "named installation scope system");
            check(snap.instances[0].identity.install_root == "/mnt/flatpak-workstation",
                  "named installation root resolved");
        }
    }

    // --- Adapter: duplicate identity produces partial ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                        "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n"
                        "org.example.App\tx86_64\tstable\t2.0\tflathub\tuser\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-dup";
        inputs.generation = 7;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::partial, "dup state partial");
        check(snap.instances.size() == 1, "dup one instance");
    }

    // --- Adapter: discover_candidates returns empty, complete ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.std_out = "";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-c";
        inputs.generation = 8;
        auto snap = adapter.discover_candidates(inputs);
        check(snap.source_id == "flatpak-cli", "candidates source id");
        check(snap.capture_id == "cap-c", "candidates capture id");
        check(snap.generation == 8, "candidates generation");
        check(snap.candidates.empty(), "no candidates");
        check(snap.state == SnapshotState::complete, "candidates complete");
    }

    // --- Adapter: valid TSV stdout with nonempty stderr diagnostic still succeeds ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.output_truncated = false;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                        "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n";
            r.std_err = "diagnostic: harmless warning on stderr\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-stderr-ok";
        inputs.generation = 11;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::complete, "stderr diagnostic: state complete");
        check(snap.instances.size() == 1, "stderr diagnostic: instance count");
        check(snap.errors.empty(), "stderr diagnostic: no errors");
        if (snap.instances.size() == 1) {
            check(snap.instances[0].software_id.value == "org.example.App", "stderr diagnostic: app id");
            check(snap.instances[0].version.descriptor.raw == "1.0", "stderr diagnostic: version");
        }
    }

    // --- Production QProcess keeps synthetic stdout TSV separate from stderr ---
    {
        QTemporaryDir fakeBin;
        check(fakeBin.isValid(), "QProcess fixture temp directory");
        if (fakeBin.isValid()) {
            QFile fakeFlatpak(fakeBin.filePath(QStringLiteral("flatpak")));
            const QByteArray script =
                "#!/bin/sh\n"
                "printf 'Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n'\n"
                "printf 'diagnostic: harmless warning on stderr\n' >&2\n";
            const bool fixtureReady =
                fakeFlatpak.open(QIODevice::WriteOnly)
                && fakeFlatpak.write(script) == script.size();
            fakeFlatpak.close();
            const bool executable = fixtureReady && QFile::setPermissions(
                fakeFlatpak.fileName(),
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            check(executable, "synthetic flatpak executable created");

            if (executable) {
                const QByteArray previousPath = qgetenv("PATH");
                const bool pathSet = qputenv("PATH", fakeBin.path().toLocal8Bit());
                check(pathSet, "fake flatpak PATH installed");
                if (pathSet) {
                    const ProcessResult result = run_flatpak_list(2000, 1024);
                    qputenv("PATH", previousPath);
                    check(result.started && result.exit_code == 0,
                          "QProcess fixture exits successfully");
                    check(!result.timed_out && !result.output_truncated,
                          "QProcess fixture stays within bounds");
                    check(result.std_err.contains(QStringLiteral("harmless warning")),
                          "QProcess captures stderr separately");
                    const ParseResult parsed = parse_flatpak_list_output(result.std_out);
                    check(parsed.ok && parsed.rows.size() == 1,
                          "QProcess stdout remains valid TSV");
                }
            }
        }
    }

    // --- Missing Flatpak executable is reported as manager_unavailable ---
    {
        QTemporaryDir emptyPath;
        check(emptyPath.isValid(), "empty PATH fixture available");
        if (emptyPath.isValid()) {
            EnvironmentGuard environment;
            const bool pathSet = qputenv("PATH", emptyPath.path().toLocal8Bit());
            check(pathSet, "empty PATH installed for executable lookup");
            if (pathSet) {
                const ProcessResult process = run_flatpak_process(
                    QStringLiteral("flatpak"), {}, 1000, 1024);
                check(process.executable_missing && !process.started,
                      "default runner distinguishes a missing CLI from launch failure");

                FlatpakCliAdapter adapter({}, 1000, 1024, {}, emptyPath.path());
                SnapshotInputs inputs;
                inputs.capture_id = "manager-unavailable-capture";
                inputs.generation = 18;
                const auto snapshot = adapter.discover_instances(inputs);
                check(snapshot.state == SnapshotState::partial
                          && snapshot.errors.size() == 1
                          && snapshot.errors.front().code == SourceErrorCode::manager_unavailable
                          && snapshot.errors.front().message == "flatpak CLI not found on PATH",
                      "missing Flatpak CLI is not represented as a successful empty scan");
            }
        }
    }

    // --- Adapter: nonzero exit reports a stable, redacted diagnostic ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 2;
            r.std_err = "https://user:secret@example.invalid diagnostic failure";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-stderr-fail";
        inputs.generation = 12;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::partial, "stderr fail: state partial");
        check(!snap.errors.empty(), "stderr fail: has error");
        if (!snap.errors.empty()) {
            check(snap.errors[0].code == SourceErrorCode::unavailable, "stderr fail: code");
            check(snap.errors[0].message == "flatpak CLI exited with status 2",
                  "stderr fail: stable exit diagnostic");
            check(snap.errors[0].message.find("user:secret") == std::string::npos,
                  "stderr fail: raw credentials are not exposed");
        }
    }

    // --- source_id ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r; r.started = true; r.exit_code = 0; r.std_out = ""; return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024);
        check(adapter.source_id() == "flatpak-cli", "source id");
    }

    // --- Candidate query uses the installed remote and commit identity, not version ordering ---
    {
        QTemporaryDir installations;
        QStringList capturedArguments;
        ProcessRunner runner = [&](const QString &, const QStringList &arguments, int, int) {
            capturedArguments = arguments;
            ProcessResult r;
            r.started = true;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tCommit\n"
                        "org.example.App\tx86_64\tstable\t1.0\t"
                        "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {}, installations.path());
        SnapshotInputs inputs;
        inputs.capture_id = "candidate-capture";
        inputs.generation = 31;
        inputs.instances = {make_flatpak_instance(
            "1.0", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")};

        const auto candidates = adapter.discover_candidates(inputs);
        check(candidates.state == SnapshotState::complete,
              "Flatpak remote candidate query completes");
        check(candidates.candidates.size() == 1,
              "changed Flatpak commit is a candidate even when the opaque version string is equal");
        check(candidates.update_checks.empty(), "candidate rows are not duplicated as no-update checks");
        check(capturedArguments.size() == 6, "candidate query has bounded exact argument list");
        if (capturedArguments.size() == 6) {
            check(capturedArguments[0] == "remote-ls", "candidate query uses remote-ls");
            check(capturedArguments[1] == "--updates", "candidate query asks Flatpak for remote update refs");
            check(capturedArguments[3]
                      == "--columns=application,arch,branch,version,commit",
                  "candidate list query follows the Flatpak column contract before resolving full commits");
            check(capturedArguments[4] == "--user", "candidate query preserves user installation scope");
            check(capturedArguments[5] == "flathub", "candidate query pins the installed origin remote");
        }
        check(!capturedArguments.contains(QStringLiteral("--cached")),
              "first candidate query does not require a pre-existing summary cache");
        if (candidates.candidates.size() == 1) {
            const auto &candidate = candidates.candidates.front();
            check(candidate.version.descriptor.raw == "1.0",
                  "candidate keeps its exact opaque source version");
            check(candidate.source_evidence.source_name == "flathub",
                  "candidate records the installed remote identity");
            check(candidate.source_evidence.channel == "stable",
                  "candidate records the exact Flatpak branch");
            check(candidate.source_evidence.remote_ref
                      == "app/org.example.App/x86_64/stable",
                  "candidate records the exact Flatpak remote ref");
            check(candidate.source_evidence.installation_scope == "user"
                      && candidate.source_evidence.installation_root
                          == "/home/tester/.local/share/flatpak",
                  "candidate records its exact installation scope and root");
            check(candidate.source_evidence.metadata_revision
                      == "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210",
                  "candidate records the source commit");
            check(!candidate.source_evidence.metadata_cached.has_value(),
                  "candidate does not claim the CLI actually used a local cache");
            check(candidate.source_evidence.freshness == MetadataFreshness::unknown,
                  "remote freshness remains unknown without source timestamp evidence");
            check(!candidate.source_evidence.checked_at_utc.empty()
                      && candidate.source_evidence.checked_at_utc.back() == 'Z',
                  "candidate records a UTC check timestamp");
            check(candidate.trust_source == TrustSource::unknown,
                  "remote name alone does not claim a trust classification");
        }

        ScanSnapshot installed;
        installed.source_id = adapter.source_id();
        installed.capture_id = inputs.capture_id;
        installed.generation = inputs.generation;
        installed.instances = inputs.instances;
        const auto core = reconcile_snapshots(installed, candidates);
        check(core.records.size() == 1
                  && core.records.front().update_status == UpdateStatus::candidate,
              "candidate evidence reconciles to the exact installed instance");
        const auto rows = to_software_row_dtos(core);
        check(rows.size() == 1
                  && !rows.front().update_source.metadata_cached.has_value()
                  && rows.front().update_source.freshness == MetadataFreshness::unknown
                  && rows.front().update_source.remote_ref
                      == "app/org.example.App/x86_64/stable"
                  && rows.front().update_source.installation_scope == "user"
                  && rows.front().update_source.installation_root
                      == "/home/tester/.local/share/flatpak",
              "unknown freshness and exact Flatpak installation evidence reach the Core DTO");
    }

    // --- Candidate rows resolve the CLI's abbreviated commit with remote-info ---
    {
        QTemporaryDir installations;
        const std::string candidateCommit =
            "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
        std::vector<QStringList> calls;
        ProcessRunner runner = [&](const QString &, const QStringList &arguments, int, int) {
            calls.push_back(arguments);
            ProcessResult r;
            r.started = true;
            r.exit_code = 0;
            if (arguments.value(0) == QStringLiteral("remote-ls")) {
                r.std_out = "org.example.App\tx86_64\tstable\t\t"
                    "fedcba987654\n";
            } else if (arguments.value(0) == QStringLiteral("remote-info")) {
                r.std_out = QString::fromStdString(candidateCommit) + QLatin1Char('\n');
            } else {
                r.exit_code = 9;
            }
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {}, installations.path());
        SnapshotInputs inputs;
        inputs.capture_id = "full-candidate-commit";
        inputs.generation = 45;
        inputs.instances = {make_flatpak_instance(
            "", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")};

        const auto checked = adapter.discover_candidates(inputs);
        check(calls.size() == 2, "abbreviated candidate commit triggers one remote-info query");
        if (calls.size() == 2) {
            check(calls[1] == QStringList{
                      QStringLiteral("remote-info"), QStringLiteral("--show-commit"),
                      QStringLiteral("--user"),
                      QStringLiteral("flathub"),
                      QStringLiteral("app/org.example.App/x86_64/stable")},
                  "remote full-commit query pins remote, scope, and exact ref");
        }
        check(checked.candidates.size() == 1,
              "a different full remote commit remains a candidate");
        if (checked.candidates.size() == 1) {
            check(checked.candidates.front().source_evidence.metadata_revision
                      == candidateCommit,
                  "candidate evidence stores the exact 64-character commit");
            check(checked.candidates.front().version.descriptor.raw.empty(),
                  "candidate with missing AppStream version remains a valid candidate");
        }
    }

    // --- An empty remote update list is not proof that the source is current ---
    {
        QTemporaryDir installations;
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tCommit\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {}, installations.path());
        SnapshotInputs inputs;
        inputs.capture_id = "up-to-date-capture";
        inputs.generation = 32;
        inputs.instances = {make_flatpak_instance(
            "1.0", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")};
        const auto checked = adapter.discover_candidates(inputs);
        check(checked.state == SnapshotState::complete && checked.candidates.empty(),
              "empty remote update list is a completed check");
        check(checked.update_checks.size() == 1
                  && checked.update_checks.front().status == UpdateCheckStatus::no_candidate,
              "empty remote update list records no-candidate rather than up-to-date");
        check(!checked.update_checks.front().source_evidence.metadata_cached.has_value()
                  && checked.update_checks.front().source_evidence.freshness
                      == MetadataFreshness::unknown,
              "empty remote update list preserves unknown cache use and freshness");
        ScanSnapshot installed;
        installed.source_id = adapter.source_id();
        installed.capture_id = inputs.capture_id;
        installed.generation = inputs.generation;
        installed.instances = inputs.instances;
        const auto core = reconcile_snapshots(installed, checked);
        check(core.records.size() == 1
                  && core.records.front().update_status == UpdateStatus::no_candidate,
              "empty remote result is distinct from both up-to-date and not checked");
    }

    // --- Matching commits prove only the observed ref revision, not freshness ---
    {
        QTemporaryDir installations;
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tCommit\n"
                        "org.example.App\tx86_64\tstable\t1.0\t"
                        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {}, installations.path());
        SnapshotInputs inputs;
        inputs.capture_id = "matching-revision-capture";
        inputs.generation = 34;
        inputs.instances = {make_flatpak_instance(
            "1.0", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")};
        const auto checked = adapter.discover_candidates(inputs);
        check(checked.update_checks.size() == 1
                  && checked.update_checks.front().status
                      == UpdateCheckStatus::observed_revision_matches,
              "matching observed remote commit is represented as observed-revision match");
        check(checked.update_checks.front().source_evidence.metadata_revision
                  == "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
                  && checked.update_checks.front().source_evidence.freshness
                      == MetadataFreshness::unknown,
              "matching commit preserves the exact observation and unknown freshness");
        ScanSnapshot installed;
        installed.source_id = adapter.source_id();
        installed.capture_id = inputs.capture_id;
        installed.generation = inputs.generation;
        installed.instances = inputs.instances;
        const auto core = reconcile_snapshots(installed, checked);
        check(core.records.size() == 1
                  && core.records.front().update_status
                      == UpdateStatus::observed_revision_matches,
              "Core never upgrades the Flatpak observation to globally up-to-date");
    }

    // --- Candidate query failure is visible as unavailable, not current ---
    {
        QTemporaryDir installations;
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            return ProcessResult{};
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {}, installations.path());
        SnapshotInputs inputs;
        inputs.capture_id = "failed-capture";
        inputs.generation = 33;
        inputs.instances = {make_flatpak_instance(
            "1.0", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")};
        const auto failed = adapter.discover_candidates(inputs);
        check(failed.state == SnapshotState::partial && failed.candidates.empty(),
              "unavailable Flatpak source is partial with no fabricated candidate");
        check(failed.update_checks.size() == 1
                  && failed.update_checks.front().status == UpdateCheckStatus::unavailable,
              "failed check is represented as unavailable");
        ScanSnapshot installed;
        installed.source_id = adapter.source_id();
        installed.capture_id = inputs.capture_id;
        installed.generation = inputs.generation;
        installed.instances = inputs.instances;
        const auto core = reconcile_snapshots(installed, failed);
        check(core.records.size() == 1
                  && core.records.front().update_status == UpdateStatus::unavailable,
              "failed source query is not presented as up-to-date");
    }

    // --- Adapter: injected runner receives exact read-only command + clamped bounds ---
    {
        QString captured_program;
        QStringList captured_args;
        int captured_timeout = 0;
        int captured_output = 0;
        ProcessRunner runner = [&](const QString &program, const QStringList &arguments,
                                   int timeout_ms, int max_output_bytes) {
            captured_program = program;
            captured_args = arguments;
            captured_timeout = timeout_ms;
            captured_output = max_output_bytes;
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n";
            return r;
        };
        FlatpakCliAdapter adapter(runner, 999999, 999999999);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-cmd";
        inputs.generation = 9;
        auto snap = adapter.discover_instances(inputs);
        check(captured_program == QStringLiteral("flatpak"), "runner program is flatpak");
        check(captured_args.size() == 3, "runner arg count");
        if (captured_args.size() == 3) {
            check(captured_args[0] == QStringLiteral("list"), "runner arg 0");
            check(captured_args[1] == QStringLiteral("--app"), "runner arg 1");
            check(captured_args[2] == QStringLiteral("--columns=application,arch,branch,version,origin,installation,active"),
                  "runner arg 2");
        }
        check(captured_timeout == 30000, "runner timeout clamped upper");
        check(captured_output == 16 * 1024 * 1024, "runner output clamped upper");
        check(snap.state == SnapshotState::complete, "cmd scan completes");
        check(snap.instances.empty(), "cmd scan empty inventory");
        check(snap.errors.empty(), "cmd scan no errors");
    }

    // --- Regression: same app ID across differing identity fields stays distinct ---
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            ProcessResult r;
            r.started = true;
            r.timed_out = false;
            r.exit_code = 0;
            r.std_out = "Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                        "org.example.App\tx86_64\tstable\t1.2.3\tflathub\tuser\n"
                        "org.example.App\taarch64\tbeta\t1.2.3\tfedora\tsystem\n"
                        "org.example.App\tx86_64\tstable\t1.2.3\tflathub\tworkstation\n";
            return r;
        };
        InstallRootResolver roots = [](const std::string &installation) -> std::optional<std::string> {
            if (installation == "user") return "/home/tester/.local/share/flatpak";
            if (installation == "system") return "/var/lib/flatpak";
            if (installation == "workstation") return "/mnt/flatpak-workstation";
            return std::nullopt;
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024, roots);
        SnapshotInputs inputs;
        inputs.capture_id = "cap-identity-regression";
        inputs.generation = 99;
        auto snap = adapter.discover_instances(inputs);
        check(snap.state == SnapshotState::complete, "identity regression: state complete");
        check(snap.instances.size() == 3, "identity regression: instance count");
        if (snap.instances.size() == 3) {
            const auto &first = snap.instances[0];
            const auto &second = snap.instances[1];
            const auto &third = snap.instances[2];

            check(first.identity.package_id == "org.example.App", "identity regression: app id 0");
            check(first.identity.architecture == "x86_64", "identity regression: arch 0");
            check(first.identity.branch == "stable", "identity regression: branch 0");
            check(first.identity.origin == "flathub", "identity regression: origin 0");
            check(first.identity.scope == "user", "identity regression: scope 0");
            check(first.identity.install_root == "/home/tester/.local/share/flatpak",
                  "identity regression: root 0");

            check(second.identity.package_id == "org.example.App", "identity regression: app id 1");
            check(second.identity.architecture == "aarch64", "identity regression: arch 1");
            check(second.identity.branch == "beta", "identity regression: branch 1");
            check(second.identity.origin == "fedora", "identity regression: origin 1");
            check(second.identity.scope == "system", "identity regression: scope 1");
            check(second.identity.install_root == "/var/lib/flatpak", "identity regression: root 1");

            check(third.identity.package_id == "org.example.App", "identity regression: app id 2");
            check(third.identity.architecture == "x86_64", "identity regression: arch 2");
            check(third.identity.branch == "stable", "identity regression: branch 2");
            check(third.identity.origin == "flathub", "identity regression: origin 2");
            check(third.identity.scope == "system", "identity regression: scope 2");
            check(third.identity.install_root == "/mnt/flatpak-workstation",
                  "identity regression: root 2");

            check(first.instance_id.value != second.instance_id.value,
                  "identity regression: arch/branch/origin/scope produce distinct id 0/1");
            check(first.instance_id.value != third.instance_id.value,
                  "identity regression: installation root produces distinct id 0/2");
            check(second.instance_id.value != third.instance_id.value,
                  "identity regression: all rows produce distinct id 1/2");

            check(first.version.descriptor.raw == "1.2.3", "identity regression: version 0 preserved");
            check(first.version.descriptor.scheme == version_id_001::VersionScheme::opaque,
                  "identity regression: version 0 opaque");
        }

        const auto candidates = adapter.discover_candidates(inputs);
        check(candidates.state == SnapshotState::complete,
              "identity regression: candidate discovery complete");
        check(candidates.candidates.empty(),
              "identity regression: opaque version does not infer update candidate");
    }

    // --- Default QProcess path: synthetic flatpak success (RAII PATH restore) ---
    {
        QTemporaryDir fakeBin;
        check(fakeBin.isValid(), "default QProcess success fixture temp directory");
        if (fakeBin.isValid()) {
            QFile fakeFlatpak(fakeBin.filePath(QStringLiteral("flatpak")));
            const QByteArray script =
                "#!/bin/sh\n"
                "printf 'Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                "org.example.App\tx86_64\tstable\t1.0\tflathub\tuser\n'\n"
                "printf 'diagnostic: harmless warning on stderr\n' >&2\n"
                "exit 0\n";
            const bool fixtureReady =
                fakeFlatpak.open(QIODevice::WriteOnly)
                && fakeFlatpak.write(script) == script.size();
            fakeFlatpak.close();
            const bool executable = fixtureReady && QFile::setPermissions(
                fakeFlatpak.fileName(),
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            check(executable, "synthetic flatpak executable created (default success)");
            check(QFile::exists(fakeFlatpak.fileName()),
                  "synthetic flatpak helper exists; no fall-through to a real executable");

            if (executable) {
                const QByteArray previousPath = qgetenv("PATH");
                EnvironmentGuard restorePath;
                QByteArray syntheticPath = fakeBin.path().toLocal8Bit();
                // PATH is intentionally restricted to the synthetic helper only.
                const bool pathSet = qputenv("PATH", syntheticPath);
                check(pathSet, "fake flatpak PATH installed (default success, temp dir first)");
                if (pathSet) {
                    const ProcessResult result = run_flatpak_list(2000, 1024);
                    check(result.started, "default QProcess success: process started");
                    check(!result.timed_out, "default QProcess success: not timed out");
                    check(!result.output_truncated, "default QProcess success: not truncated");
                    check(result.exit_code == 0, "default QProcess success: zero exit propagated");
                    check(result.std_err.contains(QStringLiteral("harmless warning")),
                          "default QProcess success: stderr captured separately");
                    const ParseResult parsed = parse_flatpak_list_output(result.std_out);
                    check(parsed.ok && parsed.rows.size() == 1,
                          "default QProcess success: stdout remains valid TSV");
                }
            }
        }
    }

    // --- Default QProcess path: synthetic flatpak nonzero exit (RAII PATH restore) ---
    {
        QTemporaryDir fakeBin;
        check(fakeBin.isValid(), "default QProcess failure fixture temp directory");
        if (fakeBin.isValid()) {
            QFile fakeFlatpak(fakeBin.filePath(QStringLiteral("flatpak")));
            const QByteArray script =
                "#!/bin/sh\n"
                "printf 'synthetic flatpak failure diagnostic\n' >&2\n"
                "exit 7\n";
            const bool fixtureReady =
                fakeFlatpak.open(QIODevice::WriteOnly)
                && fakeFlatpak.write(script) == script.size();
            fakeFlatpak.close();
            const bool executable = fixtureReady && QFile::setPermissions(
                fakeFlatpak.fileName(),
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            check(executable, "synthetic flatpak executable created (default failure)");
            check(QFile::exists(fakeFlatpak.fileName()),
                  "synthetic flatpak helper exists; no fall-through to a real executable");

            if (executable) {
                const QByteArray previousPath = qgetenv("PATH");
                EnvironmentGuard restorePath;
                QByteArray syntheticPath = fakeBin.path().toLocal8Bit();
                // PATH is intentionally restricted to the synthetic helper only.
                const bool pathSet = qputenv("PATH", syntheticPath);
                check(pathSet, "fake flatpak PATH installed (default failure, temp dir first)");
                if (pathSet) {
                    const ProcessResult result = run_flatpak_list(2000, 1024);
                    check(result.started, "default QProcess failure: process started");
                    check(!result.timed_out, "default QProcess failure: not timed out");
                    check(!result.output_truncated, "default QProcess failure: not truncated");
                    check(result.exit_code == 7,
                          "default QProcess failure: nonzero exit propagated exactly");
                    check(result.std_out.isEmpty(),
                          "default QProcess failure: no stdout on failure");
                    check(result.std_err.contains(QStringLiteral("synthetic flatpak failure diagnostic")),
                          "default QProcess failure: stderr captured");
                }
            }
        }
    }

    // --- Default QProcess path via FlatpakCliAdapter default runner: zero exit ---
    {
        QTemporaryDir fakeBin;
        check(fakeBin.isValid(), "adapter default runner success fixture temp directory");
        if (fakeBin.isValid()) {
            QFile fakeFlatpak(fakeBin.filePath(QStringLiteral("flatpak")));
            const QByteArray script =
                "#!/bin/sh\n"
                "printf 'Application\\tArch\\tBranch\\tVersion\\tOrigin\\tInstallation\\n"
                "org.example.App\\tx86_64\\tstable\\t1.0\\tflathub\\tuser\\n"
                "org.example.App\\tx86_64\\tstable\\t1.0\\tflathub\\tsystem\\n'\n"
                "exit 0\n";
            const bool fixtureReady =
                fakeFlatpak.open(QIODevice::WriteOnly)
                && fakeFlatpak.write(script) == script.size();
            fakeFlatpak.close();
            const bool executable = fixtureReady && QFile::setPermissions(
                fakeFlatpak.fileName(),
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            check(executable, "synthetic flatpak executable created (adapter success)");
            check(QFile::exists(fakeFlatpak.fileName()),
                  "synthetic flatpak helper exists; no fall-through to a real executable");

            if (executable) {
                EnvironmentGuard guard;
                const bool pathSet = qputenv("PATH", fakeBin.path().toLocal8Bit());
                const bool modeSet = qputenv("FLATPAK_SYNTHETIC_MODE", "1");
                check(pathSet && modeSet, "isolated synthetic PATH installed (adapter success)");
                if (pathSet && modeSet) {
                    FlatpakCliAdapter adapter;
                    SnapshotInputs inputs;
                    inputs.capture_id = "cap-default-qprocess-success";
                    inputs.generation = 101;
                    const auto snap = adapter.discover_instances(inputs);
                    check(snap.state == SnapshotState::complete,
                          "default QProcess success: adapter state complete");
                    check(snap.instances.size() == 2,
                          "default QProcess success: adapter discovered both rows");
                    check(snap.errors.empty(),
                          "default QProcess success: adapter has no errors");
                }
            }
        }
    }

    // --- Default QProcess path via FlatpakCliAdapter default runner: nonzero exit ---
    {
        QTemporaryDir fakeBin;
        check(fakeBin.isValid(), "adapter default runner failure fixture temp directory");
        if (fakeBin.isValid()) {
            QFile fakeFlatpak(fakeBin.filePath(QStringLiteral("flatpak")));
            const QByteArray script =
                "#!/bin/sh\n"
                "printf 'synthetic flatpak failure diagnostic\\n' >&2\n"
                "exit 7\n";
            const bool fixtureReady =
                fakeFlatpak.open(QIODevice::WriteOnly)
                && fakeFlatpak.write(script) == script.size();
            fakeFlatpak.close();
            const bool executable = fixtureReady && QFile::setPermissions(
                fakeFlatpak.fileName(),
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            check(executable, "synthetic flatpak executable created (adapter failure)");
            check(QFile::exists(fakeFlatpak.fileName()),
                  "synthetic flatpak helper exists; no fall-through to a real executable");

            if (executable) {
                EnvironmentGuard guard;
                const bool pathSet = qputenv("PATH", fakeBin.path().toLocal8Bit());
                const bool modeSet = qputenv("FLATPAK_SYNTHETIC_MODE", "1");
                check(pathSet && modeSet, "isolated synthetic PATH installed (adapter failure)");
                if (pathSet && modeSet) {
                    FlatpakCliAdapter adapter;
                    SnapshotInputs inputs;
                    inputs.capture_id = "cap-default-qprocess-failure";
                    inputs.generation = 102;
                    const auto snap = adapter.discover_instances(inputs);
                    check(snap.state == SnapshotState::partial,
                          "default QProcess failure: adapter state partial");
                    check(snap.instances.empty(),
                          "default QProcess failure: no instances on nonzero exit");
                    check(!snap.errors.empty(),
                          "default QProcess failure: adapter has an error");
                    if (!snap.errors.empty()) {
                        check(snap.errors[0].code == SourceErrorCode::unavailable,
                              "default QProcess failure: unavailable error code");
                        check(snap.errors[0].message == "flatpak CLI exited with status 7",
                              "default QProcess failure: stable redacted diagnostic");
                        check(snap.errors[0].message.find("synthetic flatpak failure diagnostic") == std::string::npos,
                              "default QProcess failure: stderr sentinel is not exposed");
                    }
                }
            }
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all tests passed\n";
    return 0;
}
