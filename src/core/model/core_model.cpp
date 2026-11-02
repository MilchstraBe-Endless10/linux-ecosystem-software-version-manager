#include "core_model.hpp"

#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ecosystem::core {
namespace {

void append_component(std::string &encoded, std::string_view value)
{
    encoded += std::to_string(value.size());
    encoded += ':';
    encoded.append(value.data(), value.size());
    encoded += '|';
}

std::string extended_qualifier(const InstanceIdentity &identity)
{
    std::string encoded = "core-instance-v1|";
    append_component(encoded, identity.qualifier);
    append_component(encoded, identity.architecture);
    append_component(encoded, identity.branch);
    append_component(encoded, identity.origin);
    return encoded;
}

VersionRelation to_relation(version_id_001::Comparison comparison)
{
    switch (comparison) {
    case version_id_001::Comparison::less: return VersionRelation::less;
    case version_id_001::Comparison::equal: return VersionRelation::equal;
    case version_id_001::Comparison::greater: return VersionRelation::greater;
    case version_id_001::Comparison::incomparable: return VersionRelation::incomparable;
    }
    return VersionRelation::unknown;
}

void add_error(CoreSnapshot &snapshot, const ScanSnapshot &source,
               SourceOperation operation, SourceErrorCode code,
               std::string message)
{
    snapshot.state = SnapshotState::partial;
    snapshot.errors.push_back({source.source_id, operation, code,
                               std::move(message), false});
}

std::string evidence_mismatch(const InstalledInstance &instance,
                              const UpdateSourceEvidence &evidence)
{
    const auto &identity = instance.identity;
    if (!evidence.installation_scope.empty()
        && evidence.installation_scope != identity.scope)
        return "source evidence installation scope does not match";
    if (!evidence.installation_root.empty()
        && evidence.installation_root != identity.install_root)
        return "source evidence installation root does not match";

    if (identity.manager == "flatpak") {
        if (evidence.installation_scope.empty() || evidence.installation_root.empty()
            || evidence.remote_ref.empty() || evidence.source_name.empty()
            || evidence.channel.empty())
            return "Flatpak source evidence is missing its remote ref or installation identity";
        const std::string expectedRef = "app/" + identity.package_id + "/"
            + identity.architecture + "/" + identity.branch;
        if (evidence.installation_scope != identity.scope
            || evidence.installation_root != identity.install_root
            || evidence.remote_ref != expectedRef
            || evidence.source_name != identity.origin
            || evidence.channel != identity.branch)
            return "Flatpak source evidence does not match the installed remote ref and installation";
    }
    return {};
}

} // namespace

IdentityResult make_install_instance_id(const InstanceIdentity &identity)
{
    version_id_001::InstallationInstanceDescriptor descriptor;
    descriptor.manager = identity.manager;
    descriptor.scope = identity.scope;
    descriptor.installRoot = identity.install_root;
    descriptor.channel = identity.channel;
    descriptor.packageId = identity.package_id;
    descriptor.instanceQualifier = extended_qualifier(identity);

    const auto result = version_id_001::makeInstallationInstanceKey(descriptor);
    if (!result.key)
        return {std::nullopt, "invalid installation identity"};
    return {InstallInstanceId{*result.key}, {}};
}

CoreSnapshot reconcile_snapshots(const ScanSnapshot &installed,
                                 const ScanSnapshot &candidates)
{
    CoreSnapshot result;
    result.state = installed.state == SnapshotState::complete
                       && candidates.state == SnapshotState::complete
        ? SnapshotState::complete : SnapshotState::partial;
    result.errors.insert(result.errors.end(), installed.errors.begin(), installed.errors.end());
    result.errors.insert(result.errors.end(), candidates.errors.begin(), candidates.errors.end());
    result.records.reserve(installed.instances.size());

    std::unordered_map<std::string, std::size_t> record_by_id;
    record_by_id.reserve(installed.instances.size());
    for (const auto &instance : installed.instances) {
        if (instance.software_id.value.empty() || instance.instance_id.value.empty()) {
            add_error(result, installed, SourceOperation::reconcile,
                      SourceErrorCode::invalid_identity,
                      "installed record has an empty software or instance ID");
            continue;
        }
        const auto [it, inserted] = record_by_id.emplace(instance.instance_id.value,
                                                         result.records.size());
        if (!inserted) {
            add_error(result, installed, SourceOperation::reconcile,
                      SourceErrorCode::duplicate_identity,
                      "duplicate installed instance ID");
            continue;
        }
        result.records.push_back({instance, std::nullopt,
                                  UpdateStatus::unknown,
                                  VersionRelation::unknown});
    }

    const bool same_capture = !installed.source_id.empty()
                           && !candidates.source_id.empty()
                           && !installed.capture_id.empty()
                           && !candidates.capture_id.empty()
                           && installed.source_id == candidates.source_id
                           && installed.capture_id == candidates.capture_id
                           && installed.generation == candidates.generation;
    if (!same_capture) {
        add_error(result, candidates, SourceOperation::reconcile,
                  SourceErrorCode::inconsistent_snapshot,
                  "installed and candidate snapshots do not share a source, capture ID, and generation");
        return result;
    }

    std::unordered_set<std::string> seen_checks;
    seen_checks.reserve(candidates.update_checks.size());
    for (const auto &check : candidates.update_checks) {
        if (check.instance_id.value.empty()) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::invalid_identity,
                      "update check has an empty instance ID");
            continue;
        }
        if (!seen_checks.insert(check.instance_id.value).second) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::duplicate_identity,
                      "duplicate update check result");
            continue;
        }
        const auto found = record_by_id.find(check.instance_id.value);
        if (found == record_by_id.end()) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::unmatched_candidate,
                      "update check has no matching installed instance");
            continue;
        }
        auto &record = result.records[found->second];
        const std::string mismatch = evidence_mismatch(record.installed,
                                                       check.source_evidence);
        if (!mismatch.empty()) {
            record.update_status = UpdateStatus::unavailable;
            record.update_source = check.source_evidence;
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::inconsistent_snapshot, mismatch);
            continue;
        }
        switch (check.status) {
        case UpdateCheckStatus::no_candidate:
            record.update_status = UpdateStatus::no_candidate;
            break;
        case UpdateCheckStatus::observed_revision_matches:
            if (record.installed.source_revision.empty()
                || check.source_evidence.metadata_revision.empty()
                || check.source_evidence.checked_at_utc.empty()
                || check.source_evidence.metadata_revision
                    != record.installed.source_revision) {
                record.update_status = UpdateStatus::unavailable;
                add_error(result, candidates, SourceOperation::reconcile,
                          SourceErrorCode::inconsistent_snapshot,
                          "observed revision match lacks an exact installed/source revision and check time");
                break;
            }
            record.update_status = UpdateStatus::observed_revision_matches;
            break;
        case UpdateCheckStatus::up_to_date:
            if (record.installed.identity.manager == "flatpak"
                && check.source_evidence.freshness != MetadataFreshness::fresh) {
                record.update_status = UpdateStatus::unavailable;
                add_error(result, candidates, SourceOperation::reconcile,
                          SourceErrorCode::inconsistent_snapshot,
                          "Flatpak up-to-date evidence is not known fresh");
                break;
            }
            record.update_status = UpdateStatus::up_to_date;
            break;
        case UpdateCheckStatus::unavailable:
            record.update_status = UpdateStatus::unavailable;
            break;
        }
        record.update_source = check.source_evidence;
    }

    std::unordered_set<std::string> seen_candidates;
    seen_candidates.reserve(candidates.candidates.size());
    for (const auto &candidate : candidates.candidates) {
        if (candidate.software_id.value.empty() || candidate.instance_id.value.empty()) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::invalid_identity,
                      "candidate has an empty software or instance ID");
            continue;
        }
        if (!seen_candidates.insert(candidate.instance_id.value).second) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::duplicate_identity,
                      "duplicate update candidate ID");
            continue;
        }
        const auto found = record_by_id.find(candidate.instance_id.value);
        if (found == record_by_id.end()) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::unmatched_candidate,
                      "candidate has no matching installed instance");
            continue;
        }
        auto &record = result.records[found->second];
        if (record.installed.software_id.value != candidate.software_id.value) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::inconsistent_snapshot,
                      "candidate software ID does not match its installed instance");
            continue;
        }
        const std::string mismatch = evidence_mismatch(record.installed,
                                                       candidate.source_evidence);
        if (!mismatch.empty()) {
            record.update_status = UpdateStatus::unavailable;
            record.update_source = candidate.source_evidence;
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::inconsistent_snapshot, mismatch);
            continue;
        }
        if (record.update_status != UpdateStatus::unknown) {
            add_error(result, candidates, SourceOperation::reconcile,
                      SourceErrorCode::inconsistent_snapshot,
                      "candidate conflicts with an explicit update check result");
            continue;
        }
        if (record.installed.identity.manager == "flatpak") {
            if (record.installed.source_revision.empty()
                || candidate.source_evidence.metadata_revision.empty()
                || candidate.source_evidence.checked_at_utc.empty()) {
                record.update_status = UpdateStatus::unavailable;
                record.update_source = candidate.source_evidence;
                add_error(result, candidates, SourceOperation::reconcile,
                          SourceErrorCode::inconsistent_snapshot,
                          "Flatpak candidate lacks installed/source commit or check-time evidence");
                continue;
            }
            if (record.installed.source_revision
                == candidate.source_evidence.metadata_revision) {
                record.update_status = UpdateStatus::observed_revision_matches;
                record.update_source = candidate.source_evidence;
                continue;
            }
        }
        record.candidate = candidate;
        record.update_status = UpdateStatus::candidate;
        record.update_source = candidate.source_evidence;
        record.version_relation = to_relation(version_id_001::compareVersions(
            record.installed.version.descriptor, candidate.version.descriptor));
    }

    if (!result.errors.empty())
        result.state = SnapshotState::partial;
    return result;
}

std::vector<SoftwareRowDto> to_software_row_dtos(const CoreSnapshot &snapshot)
{
    std::vector<SoftwareRowDto> result;
    result.reserve(snapshot.records.size());
    for (const auto &record : snapshot.records) {
        SoftwareRowDto row;
        row.software_id = record.installed.software_id.value;
        row.instance_id = record.installed.instance_id.value;
        row.installed_version = record.installed.version.descriptor.raw;
        row.update_status = record.update_status;
        row.version_relation = record.version_relation;
        row.installed_source_revision = record.installed.source_revision;
        row.installed_source_uri = record.installed.source_uri;
        if (record.candidate) {
            row.candidate_version = record.candidate->version.descriptor.raw;
            row.candidate_trust = record.candidate->trust_source;
            row.candidate_risk = record.candidate->risk;
        }
        row.update_source = record.update_source;
        row.icon_name = record.installed.icon_name;
        result.push_back(std::move(row));
    }
    return result;
}

} // namespace ecosystem::core
