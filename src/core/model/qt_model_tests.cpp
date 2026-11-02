#include "core_model.hpp"
#include "qt_software_list_model.hpp"

#include <QAbstractItemModel>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

using namespace ecosystem::core;

namespace {

int checks = 0;

void check(bool condition, const char *label)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

SoftwareRowDto dto(const std::string &software_id,
                   const std::string &instance_id,
                   const std::string &installed_version,
                   const std::string &candidate_version,
                   UpdateStatus update_status,
                   VersionRelation relation,
                   TrustSource trust,
                   UpdateRisk risk,
                   const std::string &icon_name = {})
{
    SoftwareRowDto row;
    row.software_id = software_id;
    row.instance_id = instance_id;
    row.installed_version = installed_version;
    row.candidate_version = candidate_version;
    row.update_status = update_status;
    row.version_relation = relation;
    row.candidate_trust = trust;
    row.candidate_risk = risk;
    row.icon_name = icon_name;
    return row;
}

void test_empty_model()
{
    QtSoftwareListModel model;
    check(model.rowCount() == 0, "empty model has zero rows");
    check(!model.data(QModelIndex(), QtSoftwareListModel::SoftwareIdRole).isValid(),
          "empty model returns invalid data for a null index");
    check(!model.data(model.index(0, 0), QtSoftwareListModel::SoftwareIdRole).isValid(),
          "empty model rejects an out-of-range index");
    check(model.roleNames().size() == 21, "model exposes all installed and candidate source evidence roles");

    const QHash<int, QByteArray> roles = model.roleNames();
    check(roles.value(QtSoftwareListModel::SoftwareIdRole) == "softwareId",
          "softwareId role name is stable");
    check(roles.value(QtSoftwareListModel::InstanceIdRole) == "instanceId",
          "instanceId role name is stable");
    check(roles.value(QtSoftwareListModel::InstalledVersionRole) == "installedVersion",
          "installedVersion role name is stable");
    check(roles.value(QtSoftwareListModel::CandidateVersionRole) == "candidateVersion",
          "candidateVersion role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateStatusRole) == "updateStatus",
          "updateStatus role name is stable");
    check(roles.value(QtSoftwareListModel::VersionRelationRole) == "versionRelation",
          "versionRelation role name is stable");
    check(roles.value(QtSoftwareListModel::CandidateTrustRole) == "candidateTrust",
          "candidateTrust role name is stable");
    check(roles.value(QtSoftwareListModel::CandidateRiskRole) == "candidateRisk",
          "candidateRisk role name is stable");
    check(roles.value(QtSoftwareListModel::IconNameRole) == "iconName",
          "iconName role name is stable");
    check(roles.value(QtSoftwareListModel::InstalledSourceRevisionRole) == "installedSourceRevision",
          "installed source revision role name is stable");
    check(roles.value(QtSoftwareListModel::InstalledSourceUriRole) == "installedSourceUri",
          "installed source URI role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateSourceRole) == "updateSource",
          "update source role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateSourceUriRole) == "updateSourceUri",
          "update source URI role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateChannelRole) == "updateChannel",
          "update channel role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateCheckedAtUtcRole) == "updateCheckedAtUtc",
          "update check timestamp role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateMetadataRevisionRole) == "updateMetadataRevision",
          "update metadata revision role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateFreshnessRole) == "updateFreshness",
          "update metadata freshness role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateMetadataCachedRole) == "updateMetadataCached",
          "update cached metadata role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateRemoteRefRole) == "updateRemoteRef",
          "update remote ref role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateInstallationScopeRole) == "updateInstallationScope",
          "update installation scope role name is stable");
    check(roles.value(QtSoftwareListModel::UpdateInstallationRootRole) == "updateInstallationRoot",
          "update installation root role name is stable");
}

void test_role_mapping()
{
    QtSoftwareListModel model;
    SoftwareRowDto row = dto(
        "editor", "instance-42", "1.0.0", "1.2.0",
        UpdateStatus::candidate, VersionRelation::less,
        TrustSource::official, UpdateRisk::medium);
    row.installed_source_revision = "installed-commit";
    row.installed_source_uri = "https://registry.example.invalid/pkg/-/pkg-1.0.0.tgz";
    row.update_source = {"flathub", "", "stable", "2026-11-01T12:00:00Z",
                         "candidate-commit", MetadataFreshness::unknown, true};
    row.update_source.remote_ref = "app/org.example.Editor/x86_64/stable";
    row.update_source.installation_scope = "user";
    row.update_source.installation_root = "/home/test/.local/share/flatpak";
    model.setRows({row});

    check(model.rowCount() == 1, "one row is exposed after setRows");
    const QModelIndex index = model.index(0, 0);
    check(index.isValid(), "first index is valid");

    check(model.data(index, QtSoftwareListModel::SoftwareIdRole).toString() == "editor",
          "softwareId maps to the DTO software id");
    check(model.data(index, QtSoftwareListModel::InstanceIdRole).toString() == "instance-42",
          "instanceId is preserved in the Qt adapter");
    check(model.data(index, QtSoftwareListModel::InstalledVersionRole).toString() == "1.0.0",
          "installedVersion maps to the DTO installed version");
    check(model.data(index, QtSoftwareListModel::CandidateVersionRole).toString() == "1.2.0",
          "candidateVersion maps to the DTO candidate version");
    check(model.data(index, QtSoftwareListModel::UpdateStatusRole).toString() == "candidate",
          "updateStatus maps to the DTO update status");
    check(model.data(index, QtSoftwareListModel::VersionRelationRole).toString() == "less",
          "versionRelation maps to the DTO version relation");
    check(model.data(index, QtSoftwareListModel::CandidateTrustRole).toString() == "official",
          "candidateTrust maps to the DTO candidate trust");
    check(model.data(index, QtSoftwareListModel::CandidateRiskRole).toString() == "medium",
          "candidateRisk maps to the DTO candidate risk");
    check(model.data(index, QtSoftwareListModel::InstalledSourceRevisionRole).toString() == "installed-commit",
          "installed source revision is exposed to QML");
    check(model.data(index, QtSoftwareListModel::InstalledSourceUriRole).toString()
              == "https://registry.example.invalid/pkg/-/pkg-1.0.0.tgz",
          "installed source URI is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateSourceRole).toString() == "flathub",
          "candidate source name is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateSourceUriRole).toString().isEmpty(),
          "unknown source URI remains empty");
    check(model.data(index, QtSoftwareListModel::UpdateChannelRole).toString() == "stable",
          "candidate channel is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateCheckedAtUtcRole).toString()
              == "2026-11-01T12:00:00Z",
          "candidate check time is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateMetadataRevisionRole).toString()
              == "candidate-commit",
          "candidate source revision is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateFreshnessRole).toString() == "unknown",
          "cached metadata does not falsely claim freshness");
    check(model.data(index, QtSoftwareListModel::UpdateMetadataCachedRole).toBool(),
          "cached metadata state is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateRemoteRefRole).toString()
              == "app/org.example.Editor/x86_64/stable",
          "exact update ref is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateInstallationScopeRole).toString() == "user",
          "update installation scope is exposed to QML");
    check(model.data(index, QtSoftwareListModel::UpdateInstallationRootRole).toString()
              == "/home/test/.local/share/flatpak",
          "update installation root is exposed to QML");
}

void test_invalid_index_and_role()
{
    QtSoftwareListModel model;
    model.setRows({dto("editor", "instance-1", "1.0.0", "1.1.0",
                       UpdateStatus::candidate, VersionRelation::less,
                       TrustSource::official, UpdateRisk::low)});

    check(!model.data(model.index(-1, 0), QtSoftwareListModel::SoftwareIdRole).isValid(),
          "negative row index is rejected");
    check(!model.data(model.index(1, 0), QtSoftwareListModel::SoftwareIdRole).isValid(),
          "row beyond the model size is rejected");
    check(!model.data(model.index(0, 1), QtSoftwareListModel::SoftwareIdRole).isValid(),
          "non-zero column is rejected");
    check(model.data(model.index(0, 0), 999999).isValid() == false,
          "unknown role returns invalid data");
}

void test_replace_and_reset()
{
    QtSoftwareListModel model;
    model.setRows({dto("first", "instance-first", "1.0.0", "", UpdateStatus::unknown,
                       VersionRelation::unknown, TrustSource::unknown, UpdateRisk::unknown),
                   dto("second", "instance-second", "2.0.0", "2.1.0",
                       UpdateStatus::candidate, VersionRelation::less,
                       TrustSource::community, UpdateRisk::high)});

    check(model.rowCount() == 2, "replace initial rows exposes both records");
    check(model.data(model.index(0, 0), QtSoftwareListModel::SoftwareIdRole).toString() == "first",
          "first identity is retained before replacement");
    check(model.data(model.index(1, 0), QtSoftwareListModel::InstanceIdRole).toString() == "instance-second",
          "last identity is retained before replacement");

    model.setRows({dto("replacement", "instance-replacement", "3.0.0", "3.1.0",
                       UpdateStatus::candidate, VersionRelation::greater,
                       TrustSource::vendor, UpdateRisk::critical)});
    check(model.rowCount() == 1, "setRows replaces the previous collection");
    check(model.data(model.index(0, 0), QtSoftwareListModel::SoftwareIdRole).toString() == "replacement",
          "replacement first identity is exposed");
    check(model.data(model.index(0, 0), QtSoftwareListModel::InstanceIdRole).toString() == "instance-replacement",
          "replacement instance identity is preserved");
}

void test_unknown_is_not_latest()
{
    QtSoftwareListModel model;
    const SoftwareRowDto row = dto("editor", "instance-unknown", "1.0.0", "",
                                  UpdateStatus::unknown, VersionRelation::unknown,
                                  TrustSource::unknown, UpdateRisk::unknown);
    model.setRows({row});

    check(model.data(model.index(0, 0), QtSoftwareListModel::CandidateVersionRole).toString().isEmpty(),
          "a row without an explicit candidate has no candidate version");
    check(model.data(model.index(0, 0), QtSoftwareListModel::UpdateStatusRole).toString() == "unknown",
          "unknown update status is not presented as candidate");
    check(model.data(model.index(0, 0), QtSoftwareListModel::InstalledVersionRole).toString() == "1.0.0",
          "installed version remains distinct from any candidate version");
    check(!model.data(model.index(0, 0), QtSoftwareListModel::UpdateMetadataCachedRole).isValid(),
          "unobserved cache usage remains unknown instead of becoming false");

    SoftwareRowDto matched = dto("flatpak-app", "flatpak-instance", "1.0", "",
        UpdateStatus::observed_revision_matches, VersionRelation::equal,
        TrustSource::unknown, UpdateRisk::unknown);
    model.setRows({matched});
    check(model.data(model.index(0, 0), QtSoftwareListModel::UpdateStatusRole).toString()
              == "observed_revision_matches",
          "the exact observed-revision state survives the Qt status mapping");
}

void test_ten_thousand_records()
{
    constexpr std::size_t count = 10000;
    std::vector<SoftwareRowDto> rows;
    rows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        rows.push_back(dto("software-" + std::to_string(i),
                           "instance-" + std::to_string(i),
                           "1.0.0", (i % 5 == 0) ? "1.1.0" : "",
                           (i % 5 == 0) ? UpdateStatus::candidate : UpdateStatus::unknown,
                           (i % 5 == 0) ? VersionRelation::less : VersionRelation::unknown,
                           (i % 5 == 0) ? TrustSource::official : TrustSource::unknown,
                           (i % 5 == 0) ? UpdateRisk::low : UpdateRisk::unknown));
    }

    QtSoftwareListModel model;
    model.setRows(rows);

    check(model.rowCount() == static_cast<int>(count),
          "10,000 records are exposed by the Qt model");
    check(model.data(model.index(0, 0), QtSoftwareListModel::SoftwareIdRole).toString() == "software-0",
          "first identity is retained for 10,000 records");
    check(model.data(model.index(0, 0), QtSoftwareListModel::InstanceIdRole).toString() == "instance-0",
          "first instance identity is retained for 10,000 records");
    check(model.data(model.index(static_cast<int>(count) - 1, 0),
                      QtSoftwareListModel::SoftwareIdRole).toString() == "software-9999",
          "last identity is retained for 10,000 records");
    check(model.data(model.index(static_cast<int>(count) - 1, 0),
                      QtSoftwareListModel::InstanceIdRole).toString() == "instance-9999",
          "last instance identity is retained for 10,000 records");
}

void test_set_snapshot_from_core_snapshot()
{
    QtSoftwareListModel model;

    // Matching installed/candidate pair: one software with a candidate update.
    ScanSnapshot installed{"core", "capture-1", 1, SnapshotState::complete,
        {InstalledInstance{SoftwareId{"editor"}, InstallInstanceId{"inst-1"},
            InstalledVersion{version_id_001::VersionDescriptor{"", "1.0.0"}},
            TrustSource::official, UpdateRisk::low, "icon-editor", {}}},
        {}, {}};
    installed.instances.front().source_revision = "installed-revision";
    installed.instances.front().source_uri = "https://registry.example.invalid/editor/-/editor-1.0.0.tgz";
    ScanSnapshot candidates = installed;
    UpdateCandidate candidate{SoftwareId{"editor"}, InstallInstanceId{"inst-1"},
        InstalledVersion{version_id_001::VersionDescriptor{"", "1.2.0"}},
        TrustSource::official, UpdateRisk::low};
    candidate.source_evidence = {"stable-remote", "https://repo.example.invalid/flatpak",
        "stable", "2026-11-01T12:00:00Z", "candidate-revision",
        MetadataFreshness::fresh, false};
    candidates.candidates = {candidate};
    const CoreSnapshot withCandidate = reconcile_snapshots(installed, candidates);
    model.setSnapshot(withCandidate);

    check(model.rowCount() == 1, "setSnapshot exposes one row for a matched candidate");
    const QModelIndex candidateIndex = model.index(0, 0);
    check(model.data(candidateIndex, QtSoftwareListModel::SoftwareIdRole).toString() == "editor",
          "setSnapshot preserves software id");
    check(model.data(candidateIndex, QtSoftwareListModel::InstanceIdRole).toString() == "inst-1",
          "setSnapshot preserves instance id");
    check(model.data(candidateIndex, QtSoftwareListModel::InstalledVersionRole).toString() == "1.0.0",
          "setSnapshot preserves installed version");
    check(model.data(candidateIndex, QtSoftwareListModel::CandidateVersionRole).toString() == "1.2.0",
          "setSnapshot preserves candidate version");
    check(model.data(candidateIndex, QtSoftwareListModel::UpdateStatusRole).toString() == "candidate",
          "setSnapshot marks a matched candidate as candidate");
    check(model.data(candidateIndex, QtSoftwareListModel::InstalledSourceRevisionRole).toString()
              == "installed-revision",
          "setSnapshot preserves the installed source revision");
    check(model.data(candidateIndex, QtSoftwareListModel::InstalledSourceUriRole).toString()
              == "https://registry.example.invalid/editor/-/editor-1.0.0.tgz",
          "setSnapshot preserves installed source URI evidence");
    check(model.data(candidateIndex, QtSoftwareListModel::UpdateSourceRole).toString()
              == "stable-remote",
          "setSnapshot maps source evidence into Qt roles");

    // Empty-candidate case: status stays unknown and candidate version stays empty.
    ScanSnapshot installedOnly{"core", "capture-2", 1, SnapshotState::complete,
        {InstalledInstance{SoftwareId{"editor"}, InstallInstanceId{"inst-1"},
            InstalledVersion{version_id_001::VersionDescriptor{"", "1.0.0"}},
            TrustSource::official, UpdateRisk::low, "icon-editor", {}}},
        {}, {}};
    ScanSnapshot noCandidates = installedOnly;
    noCandidates.candidates = {};
    noCandidates.update_checks = {UpdateCheckResult{
        InstallInstanceId{"inst-1"}, UpdateCheckStatus::up_to_date,
        UpdateSourceEvidence{"stable-remote", "", "stable",
                             "2026-11-01T12:05:00Z", "metadata-revision",
                             MetadataFreshness::unknown, true}}};
    const CoreSnapshot withoutCandidate = reconcile_snapshots(installedOnly, noCandidates);
    model.setSnapshot(withoutCandidate);

    check(model.rowCount() == 1, "setSnapshot exposes one row without a candidate");
    const QModelIndex unknownIndex = model.index(0, 0);
    check(model.data(unknownIndex, QtSoftwareListModel::CandidateVersionRole).toString().isEmpty(),
          "empty candidate keeps candidate version empty");
    check(model.data(unknownIndex, QtSoftwareListModel::UpdateStatusRole).toString() == "up_to_date",
          "an explicit completed check distinguishes up-to-date from not checked");
    check(model.data(unknownIndex, QtSoftwareListModel::UpdateCheckedAtUtcRole).toString()
              == "2026-11-01T12:05:00Z",
          "up-to-date checks preserve the source query timestamp");
}

} // namespace

int main()
{
    test_empty_model();
    test_role_mapping();
    test_invalid_index_and_role();
    test_replace_and_reset();
    test_unknown_is_not_latest();
    test_ten_thousand_records();
    test_set_snapshot_from_core_snapshot();
    std::cout << "OK " << checks << " checks\n";
    return 0;
}
