#pragma once

#include "version_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ecosystem::core {

struct SoftwareId {
    std::string value;
};

struct InstallInstanceId {
    std::string value;
};

struct InstanceIdentity {
    std::string manager;
    std::string scope;
    std::string install_root;
    std::string package_id;
    std::string channel;
    std::string qualifier;
    std::string architecture;
    std::string branch;
    std::string origin;
};

struct IdentityResult {
    std::optional<InstallInstanceId> id;
    std::string error;
};

IdentityResult make_install_instance_id(const InstanceIdentity &identity);

struct InstalledVersion {
    version_id_001::VersionDescriptor descriptor;
};

enum class TrustSource {
    unknown,
    official,
    vendor,
    community,
    local
};

enum class UpdateRisk {
    unknown,
    low,
    medium,
    high,
    critical
};

enum class UpdateStatus {
    unknown,
    candidate,
    no_candidate,
    observed_revision_matches,
    up_to_date,
    unavailable
};

enum class UpdateCheckStatus {
    no_candidate,
    observed_revision_matches,
    up_to_date,
    unavailable
};

enum class MetadataFreshness {
    unknown,
    fresh,
    stale
};

enum class VersionRelation {
    unknown,
    less,
    equal,
    greater,
    incomparable
};

enum class SnapshotState {
    complete,
    partial
};

enum class SourceOperation {
    discover_instances,
    discover_candidates,
    parse,
    reconcile
};

enum class SourceErrorCode {
    unavailable,
    manager_unavailable,
    malformed_data,
    invalid_identity,
    duplicate_identity,
    unmatched_candidate,
    inconsistent_snapshot,
    other
};

struct SourceError {
    std::string source_id;
    SourceOperation operation = SourceOperation::parse;
    SourceErrorCode code = SourceErrorCode::other;
    std::string message;
    bool retryable = false;
};

struct InstalledInstance {
    SoftwareId software_id;
    InstallInstanceId instance_id;
    InstalledVersion version;
    TrustSource trust_source = TrustSource::unknown;
    UpdateRisk risk = UpdateRisk::unknown;
    std::string icon_name; // opaque icon identifier; empty when absent
    InstanceIdentity identity; // preserve source identity fields for UI details
    std::string source_revision; // immutable installed source revision, e.g. Flatpak commit
    std::string source_uri; // sanitized install artifact/repository URI; not necessarily part of identity
};

struct UpdateSourceEvidence {
    std::string source_name; // exact remote, registry or repository label when known
    std::string source_uri; // sanitized endpoint; empty when not available
    std::string channel; // exact branch, dist-tag or repository channel when known
    std::string checked_at_utc; // when this query ran, not the metadata publication time
    std::string metadata_revision; // source-specific immutable revision, ETag or commit
    MetadataFreshness freshness = MetadataFreshness::unknown;
    std::optional<bool> metadata_cached; // empty when the source cannot prove cache use
    std::string remote_ref; // exact source ref, e.g. Flatpak app/id/arch/branch
    std::string installation_scope;
    std::string installation_root;
};

struct UpdateCandidate {
    SoftwareId software_id;
    InstallInstanceId instance_id;
    InstalledVersion version;
    TrustSource trust_source = TrustSource::unknown;
    UpdateRisk risk = UpdateRisk::unknown;
    UpdateSourceEvidence source_evidence;
};

// A completed check with no candidate must distinguish an inconclusive empty
// result from a source-confirmed up-to-date result.
struct UpdateCheckResult {
    InstallInstanceId instance_id;
    UpdateCheckStatus status = UpdateCheckStatus::unavailable;
    UpdateSourceEvidence source_evidence;
};

struct SnapshotInputs {
    std::string source_id;
    std::string capture_id;
    std::uint64_t generation = 0;
    SnapshotState state = SnapshotState::complete;
    std::vector<InstalledInstance> instances;
    std::vector<UpdateCandidate> candidates;
    std::vector<SourceError> errors;
};

struct ScanSnapshot {
    std::string source_id;
    std::string capture_id;
    std::uint64_t generation = 0;
    SnapshotState state = SnapshotState::complete;
    std::vector<InstalledInstance> instances;
    std::vector<UpdateCandidate> candidates;
    std::vector<SourceError> errors;
    std::vector<UpdateCheckResult> update_checks;
};

class SourceAdapter {
public:
    virtual ~SourceAdapter() = default;
    virtual std::string source_id() const = 0;
    virtual ScanSnapshot discover_instances(const SnapshotInputs &snapshot_inputs) const = 0;
    virtual ScanSnapshot discover_candidates(const SnapshotInputs &snapshot_inputs) const = 0;
};

struct CoreRecord {
    InstalledInstance installed;
    std::optional<UpdateCandidate> candidate;
    UpdateStatus update_status = UpdateStatus::unknown;
    VersionRelation version_relation = VersionRelation::unknown;
    UpdateSourceEvidence update_source;
};

struct CoreSnapshot {
    SnapshotState state = SnapshotState::complete;
    std::vector<CoreRecord> records;
    std::vector<SourceError> errors;
};

CoreSnapshot reconcile_snapshots(const ScanSnapshot &installed,
                                 const ScanSnapshot &candidates);

struct SoftwareRowDto {
    std::string software_id;
    std::string instance_id;
    std::string installed_version;
    std::string candidate_version;
    UpdateStatus update_status = UpdateStatus::unknown;
    VersionRelation version_relation = VersionRelation::unknown;
    TrustSource candidate_trust = TrustSource::unknown;
    UpdateRisk candidate_risk = UpdateRisk::unknown;
    std::string icon_name; // opaque icon identifier; empty when absent
    std::string installed_source_revision;
    std::string installed_source_uri;
    UpdateSourceEvidence update_source;
};

std::vector<SoftwareRowDto> to_software_row_dtos(const CoreSnapshot &snapshot);

} // namespace ecosystem::core
