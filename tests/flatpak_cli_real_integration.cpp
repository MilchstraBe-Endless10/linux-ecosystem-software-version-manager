#include "flatpak_cli_adapter.hpp"
#include "qt_software_list_model.hpp"

#include <QCoreApplication>
#include <QVariant>

#include <algorithm>
#include <iostream>
#include <string>

using namespace ecosystem::core;
using ecosystem::inventory::flatpak_cli::FlatpakCliAdapter;

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

QVariant roleForInstance(const QtSoftwareListModel &model,
                         const std::string &instanceId,
                         int role)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (model.data(index, QtSoftwareListModel::InstanceIdRole).toString().toStdString()
            == instanceId) {
            return model.data(index, role);
        }
    }
    return {};
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2 || (QString::fromLocal8Bit(argv[1]) != QLatin1String("candidate")
                      && QString::fromLocal8Bit(argv[1]) != QLatin1String("unavailable"))) {
        std::cerr << "usage: flatpak_cli_real_integration_tests candidate|unavailable\n";
        return 2;
    }

    const bool expectCandidate = QString::fromLocal8Bit(argv[1])
        == QLatin1String("candidate");
    const QString configDirectory = qEnvironmentVariable("FLATPAK_CONFIG_DIR")
        + QStringLiteral("/installations.d");
    FlatpakCliAdapter adapter({}, 30000, 4 * 1024 * 1024, {}, configDirectory);

    SnapshotInputs installedInputs;
    installedInputs.capture_id = "flatpak-real-cli-ci";
    installedInputs.generation = 1;
    const ScanSnapshot installed = adapter.discover_instances(installedInputs);
    check(installed.state == SnapshotState::complete,
          "real CLI inventory scan is complete");
    check(installed.errors.empty(), "real CLI inventory scan has no errors");
    check(installed.instances.size() == 3,
          "fixture contains user stable, user beta, and system stable app instances");

    const InstalledInstance *userStable = nullptr;
    const InstalledInstance *userBeta = nullptr;
    const InstalledInstance *systemStable = nullptr;
    for (const auto &instance : installed.instances) {
        check(instance.identity.package_id == "org.test.Hello",
              "fixture exposes only the synthetic application");
        check(instance.identity.architecture == "x86_64",
              "fixture architecture is preserved");
        if (instance.identity.scope == "user" && instance.identity.branch == "stable")
            userStable = &instance;
        else if (instance.identity.scope == "user" && instance.identity.branch == "beta")
            userBeta = &instance;
        else if (instance.identity.scope == "system" && instance.identity.branch == "stable")
            systemStable = &instance;
        else
            check(false, "scope and branch form one of the expected fixture identities");
    }
    check(userStable && userBeta && systemStable,
          "all user/system and stable/beta fixture identities are present");
    if (userStable && systemStable) {
        check(userStable->instance_id.value != systemStable->instance_id.value,
              "user and system installs have distinct instance identities");
        check(userStable->identity.install_root != systemStable->identity.install_root,
              "user and system installs preserve distinct roots");
    }

    SnapshotInputs candidateInputs = installedInputs;
    candidateInputs.instances = installed.instances;
    const ScanSnapshot checked = adapter.discover_candidates(candidateInputs);
    if (expectCandidate) {
        check(checked.state == SnapshotState::complete,
              "ordinary remote query succeeds with a cold summary cache");
        check(checked.errors.empty(), "candidate query has no errors");
        check(checked.candidates.size() == 2,
              "only the changed stable ref is a candidate in user and system scopes");
        check(checked.update_checks.size() == 1,
              "unchanged beta ref is represented by a completed no-candidate check");
    } else {
        check(checked.state == SnapshotState::partial,
              "unreachable remote is a partial source result");
        check(!checked.errors.empty(), "unreachable remote retains source errors");
        check(checked.candidates.empty(), "unreachable remote produces no candidate");
        check(checked.update_checks.size() == installed.instances.size(),
              "unreachable remote marks every installed instance unavailable");
        check(std::all_of(checked.update_checks.begin(), checked.update_checks.end(),
                          [](const UpdateCheckResult &checkResult) {
                              return checkResult.status == UpdateCheckStatus::unavailable
                                  && checkResult.source_evidence.freshness
                                      == MetadataFreshness::unknown
                                  && !checkResult.source_evidence.metadata_cached.has_value();
                          }),
              "unreachable remote stays unavailable with cache/freshness unknown");
    }

    const CoreSnapshot reconciled = reconcile_snapshots(installed, checked);
    check(reconciled.records.size() == installed.instances.size(),
          "Core reconciliation retains all three installation instances");
    QtSoftwareListModel model;
    model.setSnapshot(reconciled);
    check(model.rowCount() == 3, "Qt model retains all installation instances");

    if (userStable && userBeta && systemStable) {
        const QString expectedStable = expectCandidate
            ? QStringLiteral("candidate") : QStringLiteral("unavailable");
        const QString expectedBeta = expectCandidate
            ? QStringLiteral("no_candidate") : QStringLiteral("unavailable");
        for (const InstalledInstance *instance : {userStable, systemStable}) {
            check(roleForInstance(model, instance->instance_id.value,
                                  QtSoftwareListModel::UpdateStatusRole).toString()
                      == expectedStable,
                  "stable update state survives Core-to-Qt projection");
            check(roleForInstance(model, instance->instance_id.value,
                                  QtSoftwareListModel::UpdateFreshnessRole).toString()
                      == QStringLiteral("unknown"),
                  "freshness remains unknown through Qt projection");
            check(!roleForInstance(model, instance->instance_id.value,
                                   QtSoftwareListModel::UpdateMetadataCachedRole).isValid(),
                  "cache-hit evidence remains unknown through Qt projection");
        }
        check(roleForInstance(model, userBeta->instance_id.value,
                              QtSoftwareListModel::UpdateStatusRole).toString()
                  == expectedBeta,
              "unchanged branch or unavailable source is not promoted to latest");
    }

    if (expectCandidate && userStable && systemStable) {
        for (const auto &candidate : checked.candidates) {
            const InstalledInstance *installedInstance = nullptr;
            if (candidate.instance_id.value == userStable->instance_id.value)
                installedInstance = userStable;
            else if (candidate.instance_id.value == systemStable->instance_id.value)
                installedInstance = systemStable;

            check(installedInstance != nullptr,
                  "candidate belongs to one of the stable installed instances");
            check(candidate.source_evidence.freshness == MetadataFreshness::unknown,
                  "candidate freshness remains unknown");
            check(!candidate.source_evidence.metadata_cached.has_value(),
                  "candidate cache-hit evidence remains unknown");
            check(candidate.source_evidence.channel == "stable"
                      && candidate.source_evidence.remote_ref
                          == "app/org.test.Hello/x86_64/stable",
                  "candidate retains the exact branch and Flatpak ref");
            check(candidate.source_evidence.metadata_revision.size() == 64,
                  "short CLI commit is resolved to a full commit");
            if (installedInstance) {
                check(candidate.source_evidence.metadata_revision
                          != installedInstance->source_revision,
                      "candidate query observes a commit different from the installed commit");
                check(candidate.source_evidence.installation_scope
                          == installedInstance->identity.scope
                          && candidate.source_evidence.installation_root
                              == installedInstance->identity.install_root,
                      "candidate evidence remains bound to its installation");
            }
        }
    }

    if (failures == 0) {
        std::cout << "PASS: real Flatpak CLI " << argv[1]
                  << " path, scope/branch identity, Core, and Qt semantics\n";
        return 0;
    }
    std::cerr << failures << " real Flatpak CLI integration assertion(s) failed\n";
    return 1;
}
