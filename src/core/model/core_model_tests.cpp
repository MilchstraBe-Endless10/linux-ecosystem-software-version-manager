#include "core_model.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/resource.h>

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

InstalledVersion version(std::string manager, std::string raw,
                         version_id_001::VersionScheme scheme = version_id_001::VersionScheme::semver)
{
    return {{std::move(manager), std::move(raw), scheme}};
}

InstanceIdentity identity(std::string manager = "flatpak",
                          std::string scope = "user",
                          std::string root = "/mock/flatpak/user",
                          std::string package = "org.example.Editor",
                          std::string channel = "stable",
                          std::string qualifier = {},
                          std::string architecture = "x86_64",
                          std::string branch = "stable",
                          std::string origin = "flathub")
{
    return {std::move(manager), std::move(scope), std::move(root),
            std::move(package), std::move(channel), std::move(qualifier),
            std::move(architecture), std::move(branch), std::move(origin)};
}

InstallInstanceId instance_id(const InstanceIdentity &value)
{
    const auto result = make_install_instance_id(value);
    check(result.id.has_value(), "instance identity builds");
    return *result.id;
}

InstalledInstance installed(std::string software, InstallInstanceId id,
                            std::string manager, std::string raw,
                            TrustSource trust = TrustSource::unknown,
                            UpdateRisk risk = UpdateRisk::unknown)
{
    return {{std::move(software)}, std::move(id),
            version(std::move(manager), std::move(raw)), trust, risk};
}

UpdateCandidate candidate(std::string software, InstallInstanceId id,
                          std::string manager, std::string raw,
                          TrustSource trust = TrustSource::unknown,
                          UpdateRisk risk = UpdateRisk::unknown)
{
    return {{std::move(software)}, std::move(id),
            version(std::move(manager), std::move(raw)), trust, risk};
}

UpdateSourceEvidence flatpak_evidence(const InstanceIdentity &sourceIdentity,
                                      const std::string &revision)
{
    UpdateSourceEvidence evidence;
    evidence.source_name = sourceIdentity.origin;
    evidence.channel = sourceIdentity.branch;
    evidence.checked_at_utc = "2026-11-02T00:00:00Z";
    evidence.metadata_revision = revision;
    evidence.freshness = MetadataFreshness::unknown;
    evidence.remote_ref = "app/" + sourceIdentity.package_id + "/"
        + sourceIdentity.architecture + "/" + sourceIdentity.branch;
    evidence.installation_scope = sourceIdentity.scope;
    evidence.installation_root = sourceIdentity.install_root;
    return evidence;
}

class FixtureAdapter final : public SourceAdapter {
public:
    explicit FixtureAdapter(SnapshotInputs inputs) : m_inputs(std::move(inputs)) {}

    std::string source_id() const override { return m_inputs.source_id; }

    ScanSnapshot discover_instances(const SnapshotInputs &snapshot_inputs) const override
    {
        return {source_id(), snapshot_inputs.capture_id, snapshot_inputs.generation,
                snapshot_inputs.state, snapshot_inputs.instances, {}, snapshot_inputs.errors};
    }

    ScanSnapshot discover_candidates(const SnapshotInputs &snapshot_inputs) const override
    {
        return {source_id(), snapshot_inputs.capture_id, snapshot_inputs.generation,
                snapshot_inputs.state, {}, snapshot_inputs.candidates, snapshot_inputs.errors};
    }

private:
    SnapshotInputs m_inputs;
};

void test_instance_identity_dimensions()
{
    const auto base = identity();
    const auto expected = instance_id(base);
    check(expected.value == instance_id(base).value, "identity is deterministic");

    auto changed = base;
    changed.manager = "pacman";
    check(expected.value != instance_id(changed).value, "manager separates instances");
    changed = base; changed.scope = "system";
    check(expected.value != instance_id(changed).value, "scope separates instances");
    changed = base; changed.install_root = "/mock/flatpak/other";
    check(expected.value != instance_id(changed).value, "install root separates instances");
    changed = base; changed.package_id = "org.example.Other";
    check(expected.value != instance_id(changed).value, "package separates instances");
    changed = base; changed.channel = "beta";
    check(expected.value != instance_id(changed).value, "channel separates instances");
    changed = base; changed.qualifier = "project-a";
    check(expected.value != instance_id(changed).value, "qualifier separates instances");
    changed = base; changed.architecture = "aarch64";
    check(expected.value != instance_id(changed).value, "architecture separates instances");
    changed = base; changed.branch = "beta";
    check(expected.value != instance_id(changed).value, "branch separates instances");
    changed = base; changed.origin = "private-remote";
    check(expected.value != instance_id(changed).value, "origin separates instances");

    changed = base; changed.install_root = " /mock/flatpak/user ";
    const auto opaque_root = instance_id(changed);
    changed.install_root = "/mock/flatpak/user";
    check(opaque_root.value == instance_id(changed).value,
          "base identity trims surrounding root whitespace but does not resolve paths");

    changed = base; changed.install_root.clear();
    check(!make_install_instance_id(changed).id,
          "missing required install root is rejected");
}

void test_flatpak_and_pacman_mock_contracts()
{
    const auto flatpak_user = instance_id(identity(
        "flatpak", "user", "/mock/flatpak/user", "org.example.Editor",
        "stable", {}, "x86_64", "stable", "flathub"));
    const auto flatpak_games = instance_id(identity(
        "flatpak", "installation:games", "/mock/flatpak/games", "org.example.Editor",
        "beta", {}, "aarch64", "beta", "private"));
    const auto pacman_system = instance_id(identity(
        "pacman", "system", "/mock/pacman/root", "editor",
        "core", {}, "x86_64", "", "core"));

    check(flatpak_user.value != flatpak_games.value,
          "Flatpak user and named installation are distinct");
    check(flatpak_user.value != pacman_system.value,
          "same logical software from different managers remains a distinct instance");

    SnapshotInputs flatpak_inputs{
        "flatpak", "flatpak-capture-1", 1, SnapshotState::complete,
        {installed("editor", flatpak_user, "flatpak", "1.0.0", TrustSource::official),
         installed("editor", flatpak_games, "flatpak", "1.0.0", TrustSource::community)},
        {candidate("editor", flatpak_user, "flatpak", "1.0.0",
                   TrustSource::official, UpdateRisk::low)},
        {}
    };
    FixtureAdapter flatpak_adapter(flatpak_inputs);
    const auto flatpak_result = reconcile_snapshots(
        flatpak_adapter.discover_instances(flatpak_inputs),
        flatpak_adapter.discover_candidates(flatpak_inputs));
    check(flatpak_result.records.size() == 2, "Flatpak fixture keeps both install instances");
    check(flatpak_result.records[0].update_status == UpdateStatus::candidate,
          "explicit Flatpak candidate is visible even when display versions are equal");
    check(flatpak_result.records[0].version_relation == VersionRelation::equal,
          "version relation is informational and separate from candidate membership");
    check(flatpak_result.records[1].update_status == UpdateStatus::unknown,
          "no candidate is unknown, not an assertion that it is current");

    SnapshotInputs pacman_inputs{
        "pacman", "pacman-capture-1", 1, SnapshotState::complete,
        {installed("editor", pacman_system, "pacman", "1.0.0", TrustSource::official)},
        {candidate("editor", pacman_system, "pacman", "1.2.0",
                   TrustSource::official, UpdateRisk::medium)},
        {}
    };
    FixtureAdapter pacman_adapter(pacman_inputs);
    const auto pacman_result = reconcile_snapshots(
        pacman_adapter.discover_instances(pacman_inputs),
        pacman_adapter.discover_candidates(pacman_inputs));
    check(pacman_result.records.size() == 1
          && pacman_result.records[0].version_relation == VersionRelation::less,
          "pacman mock candidate is represented without executing pacman");
}

void test_snapshot_errors_and_consistency()
{
    const auto id = instance_id(identity());
    SnapshotInputs input{
        "flatpak", "capture-a", 7, SnapshotState::complete,
        {installed("editor", id, "flatpak", "1.0.0")},
        {candidate("editor", id, "flatpak", "1.1.0")},
        {}
    };
    FixtureAdapter adapter(input);
    auto installed_snapshot = adapter.discover_instances(input);
    auto candidate_snapshot = adapter.discover_candidates(input);

    candidate_snapshot.capture_id = "capture-b";
    auto inconsistent = reconcile_snapshots(installed_snapshot, candidate_snapshot);
    check(inconsistent.state == SnapshotState::partial
          && inconsistent.records[0].update_status == UpdateStatus::unknown,
          "mismatched capture IDs cannot create a candidate join");

    candidate_snapshot = adapter.discover_candidates(input);
    candidate_snapshot.generation = 8;
    const auto stale_generation = reconcile_snapshots(installed_snapshot, candidate_snapshot);
    check(stale_generation.state == SnapshotState::partial
          && stale_generation.records[0].update_status == UpdateStatus::unknown,
          "mismatched source generations cannot create a candidate join");

    candidate_snapshot = adapter.discover_candidates(input);
    candidate_snapshot.state = SnapshotState::partial;
    candidate_snapshot.errors.push_back({"flatpak", SourceOperation::discover_candidates,
                                         SourceErrorCode::unavailable,
                                         "synthetic remote unavailable", true});
    const auto partial = reconcile_snapshots(installed_snapshot, candidate_snapshot);
    check(partial.state == SnapshotState::partial && partial.errors.size() == 1,
          "partial source failure is retained in the merged snapshot");

    candidate_snapshot = adapter.discover_candidates(input);
    candidate_snapshot.candidates.push_back(candidate_snapshot.candidates.front());
    const auto duplicate = reconcile_snapshots(installed_snapshot, candidate_snapshot);
    check(duplicate.state == SnapshotState::partial
          && duplicate.errors.back().code == SourceErrorCode::duplicate_identity,
          "duplicate update candidates are reported");

    candidate_snapshot = adapter.discover_candidates(input);
    candidate_snapshot.candidates.front().software_id.value = "different-software";
    const auto wrong_software = reconcile_snapshots(installed_snapshot, candidate_snapshot);
    check(wrong_software.records[0].update_status == UpdateStatus::unknown
          && wrong_software.errors.back().code == SourceErrorCode::inconsistent_snapshot,
          "matching instance ID cannot override a mismatched SoftwareId");

    candidate_snapshot = adapter.discover_candidates(input);
    candidate_snapshot.candidates.front().version.descriptor.scheme = version_id_001::VersionScheme::opaque;
    const auto opaque = reconcile_snapshots(installed_snapshot, candidate_snapshot);
    check(opaque.records[0].update_status == UpdateStatus::candidate
          && opaque.records[0].version_relation == VersionRelation::incomparable,
          "opaque versions remain explicit candidates with incomparable relation");
}

void test_flatpak_revision_evidence_is_scoped()
{
    InstanceIdentity sourceIdentity = identity();
    const auto id = instance_id(sourceIdentity);
    InstalledInstance local = installed("editor", id, "flatpak", "1.0");
    local.identity = sourceIdentity;
    local.source_revision = "commit-a";

    ScanSnapshot installedSnapshot{"flatpak-cli", "scoped-capture", 1,
        SnapshotState::complete, {local}, {}, {}};
    ScanSnapshot checkedSnapshot{"flatpak-cli", "scoped-capture", 1,
        SnapshotState::complete, {}, {}, {}};
    auto addCheck = [&](UpdateCheckStatus status, UpdateSourceEvidence evidence) {
        checkedSnapshot.update_checks = {{id, status, std::move(evidence)}};
        return reconcile_snapshots(installedSnapshot, checkedSnapshot);
    };

    const auto observed = addCheck(UpdateCheckStatus::observed_revision_matches,
                                   flatpak_evidence(sourceIdentity, "commit-a"));
    check(observed.records.size() == 1
              && observed.records.front().update_status
                  == UpdateStatus::observed_revision_matches
              && observed.records.front().update_source.freshness == MetadataFreshness::unknown,
          "matching Flatpak commit is scoped as an observation, with freshness independent");

    const auto rejectEvidence = [&](const UpdateSourceEvidence &evidence,
                                    const std::string &label) {
        const auto result = addCheck(UpdateCheckStatus::no_candidate, evidence);
        check(result.state == SnapshotState::partial && result.records.size() == 1
                  && result.records.front().update_status == UpdateStatus::unavailable
                  && !result.errors.empty()
                  && result.errors.back().code == SourceErrorCode::inconsistent_snapshot,
              label.c_str());
    };

    auto wrong = flatpak_evidence(sourceIdentity, "commit-a");
    wrong.source_name = "other-remote";
    rejectEvidence(wrong, "remote evidence from a different origin is rejected");
    wrong = flatpak_evidence(sourceIdentity, "commit-a");
    wrong.remote_ref = "app/org.example.Editor/x86_64/beta";
    wrong.channel = "beta";
    rejectEvidence(wrong, "a different Flatpak branch ref is rejected");
    wrong = flatpak_evidence(sourceIdentity, "commit-a");
    wrong.remote_ref = "app/org.example.Editor/aarch64/stable";
    rejectEvidence(wrong, "a different Flatpak architecture ref is rejected");
    wrong = flatpak_evidence(sourceIdentity, "commit-a");
    wrong.installation_scope = "system";
    rejectEvidence(wrong, "a different Flatpak installation scope is rejected");
    wrong = flatpak_evidence(sourceIdentity, "commit-a");
    wrong.installation_root = "/mock/flatpak/other";
    rejectEvidence(wrong, "a different Flatpak installation root is rejected");
    wrong = flatpak_evidence(sourceIdentity, "commit-a");
    wrong.remote_ref.clear();
    rejectEvidence(wrong, "missing Flatpak ref evidence is rejected");

    auto unprovenLatest = addCheck(UpdateCheckStatus::up_to_date,
                                   flatpak_evidence(sourceIdentity, "commit-a"));
    check(unprovenLatest.records.front().update_status == UpdateStatus::unavailable
              && !unprovenLatest.errors.empty(),
          "unknown-freshness Flatpak evidence cannot claim up-to-date");

    checkedSnapshot.update_checks.clear();
    auto mismatchedCandidate = candidate("editor", id, "flatpak", "1.1");
    mismatchedCandidate.source_evidence = flatpak_evidence(sourceIdentity, "commit-b");
    mismatchedCandidate.source_evidence.installation_root = "/mock/flatpak/other";
    checkedSnapshot.candidates = {mismatchedCandidate};
    const auto candidateResult = reconcile_snapshots(installedSnapshot, checkedSnapshot);
    check(candidateResult.records.front().update_status == UpdateStatus::unavailable
              && !candidateResult.errors.empty(),
          "candidate from a different Flatpak installation root is rejected");

    checkedSnapshot.candidates.front().source_evidence = flatpak_evidence(sourceIdentity, "commit-a");
    const auto sameCommitCandidate = reconcile_snapshots(installedSnapshot, checkedSnapshot);
    check(sameCommitCandidate.records.front().update_status
              == UpdateStatus::observed_revision_matches
              && !sameCommitCandidate.records.front().candidate,
          "a Flatpak candidate with the installed commit is downgraded to an observation");
}

void test_qt_dto_boundary()
{
    const auto id = instance_id(identity());
    SnapshotInputs input{
        "flatpak", "capture-ui", 2, SnapshotState::complete,
        {installed("editor", id, "flatpak", "1.0.0")},
        {candidate("editor", id, "flatpak", "1.1.0",
                   TrustSource::official, UpdateRisk::low)},
        {}
    };
    FixtureAdapter adapter(input);
    const auto snapshot = reconcile_snapshots(adapter.discover_instances(input),
                                              adapter.discover_candidates(input));
    const auto rows = to_software_row_dtos(snapshot);
    check(rows.size() == 1 && rows[0].software_id == "editor",
          "pure DTO projection preserves SoftwareId");
    check(rows[0].candidate_version == "1.1.0"
          && rows[0].update_status == UpdateStatus::candidate,
          "pure DTO exposes candidate status to a future Qt model");
    check(rows[0].candidate_trust == TrustSource::official
          && rows[0].candidate_risk == UpdateRisk::low,
          "pure DTO carries trust and risk dimensions");
}

void test_ten_thousand_records()
{
    constexpr std::size_t count = 10000;
    ScanSnapshot installed_snapshot{
        "synthetic", "large-capture", 1, SnapshotState::complete, {}, {}, {}};
    ScanSnapshot candidate_snapshot{
        "synthetic", "large-capture", 1, SnapshotState::complete, {}, {}, {}};
    installed_snapshot.instances.reserve(count);
    candidate_snapshot.candidates.reserve(count / 5);

    for (std::size_t i = 0; i < count; ++i) {
        const auto suffix = std::to_string(i);
        const InstallInstanceId id{"instance-" + suffix};
        installed_snapshot.instances.push_back(
            installed("software-" + suffix, id, "mock", "1.0.0"));
        if (i % 5 == 0)
            candidate_snapshot.candidates.push_back(
                candidate("software-" + suffix, id, "mock", "1.1.0"));
    }

    const auto start = std::chrono::steady_clock::now();
    const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    const auto dto_start = std::chrono::steady_clock::now();
    const auto rows = to_software_row_dtos(merged);
    const auto dto_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - dto_start).count();
    rusage usage{};
    check(getrusage(RUSAGE_SELF, &usage) == 0, "read process peak RSS");
    check(merged.records.size() == count, "10,000 installed records are retained");
    check(rows.size() == count, "10,000 records project into the UI DTO boundary");
    std::size_t matched = 0;
    for (const auto &record : merged.records)
        matched += record.update_status == UpdateStatus::candidate;
    check(matched == count / 5, "10,000-row reconciliation matches explicit candidates");
    check(usage.ru_maxrss < 64 * 1024, "10,000-row peak RSS remains below 64 MiB");
    std::cout << "10k records: reconcile_ms=" << elapsed
              << " dto_projection_ms=" << dto_elapsed
              << " peak_rss_kib=" << usage.ru_maxrss << '\n';
}

void test_invalid_installed_identity()
{
    const auto id = instance_id(identity());
    SnapshotInputs input{
        "flatpak", "capture-invalid-installed", 3, SnapshotState::complete,
        {installed("editor", id, "flatpak", "1.0.0"),
         installed("", id, "flatpak", "1.0.0"),
         installed("editor", InstallInstanceId{""}, "flatpak", "1.0.0")},
        {candidate("editor", id, "flatpak", "1.1.0")},
        {}
    };
    FixtureAdapter adapter(input);
    const auto merged = reconcile_snapshots(adapter.discover_instances(input),
                                            adapter.discover_candidates(input));
    check(merged.state == SnapshotState::partial,
          "invalid installed identity marks the merged result partial");
    check(merged.records.size() == 1,
          "invalid installed rows do not appear as valid records");
    bool has_invalid_identity = false;
    for (const auto &error : merged.errors) {
        if (error.code == SourceErrorCode::invalid_identity)
            has_invalid_identity = true;
    }
    check(has_invalid_identity,
          "invalid installed identity is reported as invalid_identity");
    check(merged.records[0].update_status == UpdateStatus::candidate,
          "valid installed row still reconciles its candidate");
}

void test_unmatched_update_candidate()
{
    const auto id = instance_id(identity());
    const auto missing_id = InstallInstanceId{"instance-that-does-not-exist"};
    SnapshotInputs input{
        "flatpak", "capture-unmatched-candidate", 4, SnapshotState::complete,
        {installed("editor", id, "flatpak", "1.0.0")},
        {candidate("editor", id, "flatpak", "1.1.0"),
         candidate("editor", missing_id, "flatpak", "1.2.0")},
        {}
    };
    FixtureAdapter adapter(input);
    const auto merged = reconcile_snapshots(adapter.discover_instances(input),
                                            adapter.discover_candidates(input));
    check(merged.state == SnapshotState::partial,
          "unmatched update candidate marks the merged result partial");
    check(merged.records.size() == 1,
          "unmatched candidate does not create or remove installed rows");
    check(merged.records[0].installed.version.descriptor.raw == "1.0.0",
          "unmatched candidate leaves installed row unchanged");
    check(merged.records[0].update_status == UpdateStatus::candidate,
          "matched candidate still updates its installed row");
    bool has_unmatched_candidate = false;
    for (const auto &error : merged.errors) {
        if (error.code == SourceErrorCode::unmatched_candidate)
            has_unmatched_candidate = true;
    }
    check(has_unmatched_candidate,
          "unmatched candidate is reported as unmatched_candidate");
}

void test_missing_provenance_prevents_candidate_join()
{
    const auto id = instance_id(identity());

    auto has_inconsistent = [](const CoreSnapshot &snapshot) {
        for (const auto &error : snapshot.errors) {
            if (error.code == SourceErrorCode::inconsistent_snapshot)
                return true;
        }
        return false;
    };

    {
        ScanSnapshot installed_snapshot{"", "capture-p", 1, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"", "capture-p", 1, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1, "empty source IDs retain installed records");
        check(merged.records[0].update_status == UpdateStatus::unknown,
              "empty source IDs do not join candidates");
        check(merged.state == SnapshotState::partial,
              "empty source IDs mark the result partial");
        check(has_inconsistent(merged),
              "empty source IDs emit inconsistent_snapshot");
    }

    {
        ScanSnapshot installed_snapshot{"flatpak", "", 1, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"flatpak", "", 1, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1, "empty capture IDs retain installed records");
        check(merged.records[0].update_status == UpdateStatus::unknown,
              "empty capture IDs do not join candidates");
        check(merged.state == SnapshotState::partial,
              "empty capture IDs mark the result partial");
        check(has_inconsistent(merged),
              "empty capture IDs emit inconsistent_snapshot");
    }

    {
        ScanSnapshot installed_snapshot{"flatpak", "capture-p", 1, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"", "capture-p", 1, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1,
              "one-sided missing source ID retains installed records");
        check(merged.records[0].update_status == UpdateStatus::unknown,
              "one-sided missing source ID does not join candidates");
        check(has_inconsistent(merged),
              "one-sided missing source ID emits inconsistent_snapshot");
    }

    {
        ScanSnapshot installed_snapshot{"", "capture-p", 1, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"flatpak", "capture-p", 1, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1,
              "reverse one-sided missing source ID retains installed records");
        check(merged.records[0].update_status == UpdateStatus::unknown,
              "reverse one-sided missing source ID does not join candidates");
        check(has_inconsistent(merged),
              "reverse one-sided missing source ID emits inconsistent_snapshot");
    }

    {
        ScanSnapshot installed_snapshot{"flatpak", "capture-p", 1, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"flatpak", "", 1, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1,
              "one-sided missing capture ID retains installed records");
        check(merged.records[0].update_status == UpdateStatus::unknown,
              "one-sided missing capture ID does not join candidates");
        check(has_inconsistent(merged),
              "one-sided missing capture ID emits inconsistent_snapshot");
    }

    {
        ScanSnapshot installed_snapshot{"flatpak", "capture-zero", 0, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"flatpak", "capture-zero", 0, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1,
              "generation zero keeps installed records");
        check(merged.records[0].update_status == UpdateStatus::candidate,
              "generation zero with complete provenance joins candidates");
        check(!has_inconsistent(merged),
              "generation zero with complete provenance is not inconsistent");
    }

    {
        ScanSnapshot installed_snapshot{"flatpak", "capture-valid", 3, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"flatpak", "capture-valid", 3, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1,
              "valid provenance keeps installed records");
        check(merged.records[0].update_status == UpdateStatus::candidate,
              "valid provenance joins candidate");
        check(merged.records[0].version_relation == VersionRelation::less,
              "valid provenance preserves version relation");
    }
}

void test_existing_mismatched_capture_and_generation()
{
    const auto id = instance_id(identity());

    auto has_inconsistent = [](const CoreSnapshot &snapshot) {
        for (const auto &error : snapshot.errors) {
            if (error.code == SourceErrorCode::inconsistent_snapshot)
                return true;
        }
        return false;
    };

    {
        ScanSnapshot installed_snapshot{"flatpak", "capture-a", 7, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"flatpak", "capture-b", 7, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1, "mismatched capture keeps installed records");
        check(merged.records[0].update_status == UpdateStatus::unknown,
              "mismatched capture does not join candidate");
        check(has_inconsistent(merged), "mismatched capture emits inconsistent_snapshot");
    }

    {
        ScanSnapshot installed_snapshot{"flatpak", "capture-a", 7, SnapshotState::complete,
                                       {installed("editor", id, "flatpak", "1.0.0")}, {}, {}};
        ScanSnapshot candidate_snapshot{"flatpak", "capture-a", 8, SnapshotState::complete,
                                       {}, {candidate("editor", id, "flatpak", "1.1.0")}, {}};
        const auto merged = reconcile_snapshots(installed_snapshot, candidate_snapshot);
        check(merged.records.size() == 1, "mismatched generation keeps installed records");
        check(merged.records[0].update_status == UpdateStatus::unknown,
              "mismatched generation does not join candidate");
        check(has_inconsistent(merged), "mismatched generation emits inconsistent_snapshot");
    }
}

} // namespace

int main()
{
    test_instance_identity_dimensions();
    test_flatpak_and_pacman_mock_contracts();
    test_snapshot_errors_and_consistency();
    test_flatpak_revision_evidence_is_scoped();
    test_qt_dto_boundary();
    test_ten_thousand_records();
    test_invalid_installed_identity();
    test_unmatched_update_candidate();
    test_missing_provenance_prevents_candidate_join();
    test_existing_mismatched_capture_and_generation();
    std::cout << "OK " << checks << " checks\n";
    return 0;
}
