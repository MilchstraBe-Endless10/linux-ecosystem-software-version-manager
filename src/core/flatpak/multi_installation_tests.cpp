#include "flatpak_cli_adapter.hpp"
#include "core_model.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include <cassert>
#include <iostream>
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
}

int main() {
    // Missing enumeration retains the successfully discovered default fixture row.
    {
        std::vector<QStringList> calls;
        ProcessRunner runner = [&](const QString &, const QStringList &arguments, int, int) {
            calls.push_back(arguments);
            return ProcessResult{true, false, false, 0,
                QStringLiteral("Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.Default\tx86_64\tstable\t1.0\torigin\tsystem\n"),
                {}};
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {},
                                 QStringLiteral("/definitely/missing/installations.d"));
        SnapshotInputs inputs;
        const auto snapshot = adapter.discover_instances(inputs);
        check(calls.size() == 1, "default query still runs when enumeration fails");
        check(snapshot.instances.size() == 1, "default fixture row retained");
        check(snapshot.errors.empty(), "absent installations.d adds no error");
        check(snapshot.state == SnapshotState::complete, "complete state when installations.d is absent");
    }

    // Duplicate rows are skipped without duplicate instances.
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            return ProcessResult{true, false, false, 0,
                QStringLiteral("Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.App\tx86_64\tstable\t1.0\torigin\tsystem\n"
                               "org.example.App\tx86_64\tstable\t1.0\torigin\tsystem\n"),
                {}};
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {},
                                 QStringLiteral("/definitely/missing/installations.d"));
        SnapshotInputs inputs;
        const auto snapshot = adapter.discover_instances(inputs);
        check(snapshot.instances.size() == 1, "duplicate identity skipped");
        check(!snapshot.errors.empty(), "duplicate identity reported");
    }

    // Exact arguments for default and named queries.
    {
        QTemporaryDir dir;
        check(dir.isValid(), "temp dir");
        QFile file(dir.path() + "/alpha.conf");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Installation \"alpha\"]\nPath=/custom/alpha\n";
        }
        std::vector<QStringList> calls;
        ProcessRunner runner = [&](const QString &, const QStringList &arguments, int, int) {
            calls.push_back(arguments);
            QString installation = QStringLiteral("system");
            for (const QString &arg : arguments) {
                if (arg.startsWith(QStringLiteral("--installation=")))
                    installation = arg.mid(QStringLiteral("--installation=").size());
            }
            return ProcessResult{true, false, false, 0,
                QStringLiteral("Application\tArch\tBranch\tVersion\tOrigin\tInstallation\n"
                               "org.example.App\tx86_64\tstable\t1.0\torigin\t") + installation + QStringLiteral("\n"),
                {}};
        };
        InstallRootResolver resolver = [](const std::string &installation) {
            if (installation == "alpha") return std::optional<std::string>("/custom/alpha");
            return std::optional<std::string>("/var/lib/flatpak");
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, resolver, dir.path());
        SnapshotInputs inputs;
        const auto snapshot = adapter.discover_instances(inputs);
        check(calls.size() == 2, "default plus named query count");
        if (calls.size() == 2) {
            check(calls[0] == QStringList{
                QStringLiteral("list"), QStringLiteral("--app"),
                QStringLiteral("--columns=application,arch,branch,version,origin,installation,active")},
                "default query arguments");
            check(calls[1] == QStringList{
                QStringLiteral("list"), QStringLiteral("--app"),
                QStringLiteral("--columns=application,arch,branch,version,origin,installation,active"),
                QStringLiteral("--installation=alpha")},
                "named query arguments");
        }
        check(snapshot.instances.size() == 2, "default and named distinct instances");
        check(snapshot.errors.empty(), "no errors for successful queries");
    }

    // Real QProcess-backed execution with a temporary synthetic executable.
    {
        QTemporaryDir dir;
        check(dir.isValid(), "temp dir");
        QFile conf(dir.path() + "/installations.d/alpha.conf");
        QDir().mkpath(dir.path() + "/installations.d");
        if (conf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&conf);
            out << "[Installation \"alpha\"]\nPath=/custom/alpha\n";
        }
        QFile script(dir.path() + "/fake-flatpak");
        if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&script);
            out << "#!/bin/sh\n";
            out << "inst=system\n";
            out << "for a in \"$@\"; do case \"$a\" in --installation=*) inst=${a#--installation=};; esac; done\n";
            out << "printf 'Application\\tArch\\tBranch\\tVersion\\tOrigin\\tInstallation\\n'\n";
            out << "printf 'org.example.App\\tx86_64\\tstable\\t1.0\\torigin\\t%s\\n' \"$inst\"\n";
            out.flush();
        }
        script.close();
        check(script.error() == QFile::NoError, "synthetic script write succeeded");
        script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                              QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
        ProcessRunner runner = [&](const QString &, const QStringList &arguments, int timeout, int outputLimit) {
            return run_flatpak_process(script.fileName(), arguments, timeout, outputLimit);
        };
        InstallRootResolver resolver = [](const std::string &installation) {
            if (installation == "alpha") return std::optional<std::string>("/custom/alpha");
            return std::optional<std::string>("/var/lib/flatpak");
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, resolver, dir.path() + "/installations.d");
        SnapshotInputs inputs;
        const auto snapshot = adapter.discover_instances(inputs);
        if (snapshot.instances.size() != 2 || !snapshot.errors.empty()) {
            std::cerr << "QProcess synthetic: instances=" << snapshot.instances.size()
                      << " errors=" << snapshot.errors.size() << "\n";
            for (const auto &error : snapshot.errors)
                std::cerr << "  " << error.message << "\n";
        }
        check(snapshot.instances.size() == 2, "QProcess synthetic default and named instances");
        check(snapshot.errors.empty(), "QProcess synthetic run has no errors");
    }

    // Malformed output fails closed for that query.
    {
        ProcessRunner runner = [](const QString &, const QStringList &, int, int) {
            return ProcessResult{true, false, false, 0,
                                QStringLiteral("Application Arch Branch\nbad row\n"), {}};
        };
        FlatpakCliAdapter adapter(runner, 1000, 1024 * 1024, {},
                                 QStringLiteral("/definitely/missing/installations.d"));
        SnapshotInputs inputs;
        const auto snapshot = adapter.discover_instances(inputs);
        check(snapshot.instances.empty(), "malformed output yields no instances");
        check(!snapshot.errors.empty(), "malformed output reported");
    }

    // A present non-directory installations path remains explicit error evidence,
    // distinct from an absent optional directory.
    {
        QTemporaryDir parent;
        check(parent.isValid(), "temp parent");
        const QString notADirectory = parent.path() + "/not-a-directory";
        QFile file(notADirectory);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            file.close();
        std::string error;
        const auto names = enumerate_flatpak_system_installations(notADirectory, error);
        check(names.empty(), "non-directory installations path yields no names");
        check(!error.empty(), "non-directory installations path reported");
    }

    // Unreadable configured .conf entries are reported instead of being silently
    // omitted by a read-access directory filter. A valid sibling name remains
    // available when permission denial can actually be represented.
    {
        QTemporaryDir dir;
        check(dir.isValid(), "temp dir for unreadable config");
        QFile readable(dir.path() + "/alpha.conf");
        if (readable.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&readable);
            out << "[Installation \"alpha\"]\nPath=/custom/alpha\n";
            readable.close();
        }
        QFile unreadable(dir.path() + "/unreadable.conf");
        if (unreadable.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&unreadable);
            out << "[Installation \"beta\"]\nPath=/custom/beta\n";
            unreadable.close();
            unreadable.setPermissions(QFile::Permissions());
        }
        const QFileInfo unreadableInfo(dir.path() + "/unreadable.conf");
        if (!unreadableInfo.isReadable()) {
            std::string error;
            const auto names = enumerate_flatpak_system_installations(dir.path(), error);
            bool hasAlpha = false;
            for (const auto &name : names)
                if (name == "alpha") hasAlpha = true;
            check(hasAlpha, "valid sibling retained when an unreadable config exists");
            check(!error.empty(), "unreadable config is reported");
        } else {
            std::cerr << "SKIP/LIMITATION: this environment cannot represent an "
                         "unreadable .conf file for QFileInfo::isReadable; "
                         "the unreadable-file branch was not exercised.\\n";
        }
    }

    if (failures == 0) {
        std::cout << "multi_installation_tests: PASS\n";
        return 0;
    }
    std::cerr << "multi_installation_tests: " << failures << " failures\n";
    return 1;
}
