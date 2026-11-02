#include "qt_software_list_model.hpp"

#include <QString>

namespace ecosystem::core {

QtSoftwareListModel::QtSoftwareListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void QtSoftwareListModel::setRows(const std::vector<SoftwareRowDto> &rows)
{
    beginResetModel();
    rows_ = rows;
    endResetModel();
}

void QtSoftwareListModel::setSnapshot(const CoreSnapshot &snapshot)
{
    setRows(to_software_row_dtos(snapshot));
}

int QtSoftwareListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(rows_.size());
}

QVariant QtSoftwareListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || static_cast<std::size_t>(index.row()) >= rows_.size()) {
        return {};
    }

    const SoftwareRowDto &row = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case SoftwareIdRole:
        return QString::fromStdString(row.software_id);
    case InstanceIdRole:
        return QString::fromStdString(row.instance_id);
    case InstalledVersionRole:
        return QString::fromStdString(row.installed_version);
    case CandidateVersionRole:
        return QString::fromStdString(row.candidate_version);
    case UpdateStatusRole:
        return toString(row.update_status);
    case VersionRelationRole:
        return toString(row.version_relation);
    case CandidateTrustRole:
        return toString(row.candidate_trust);
    case CandidateRiskRole:
        return toString(row.candidate_risk);
    case IconNameRole:
        return QString::fromStdString(row.icon_name);
    case InstalledSourceRevisionRole:
        return QString::fromStdString(row.installed_source_revision);
    case InstalledSourceUriRole:
        return QString::fromStdString(row.installed_source_uri);
    case UpdateSourceRole:
        return QString::fromStdString(row.update_source.source_name);
    case UpdateSourceUriRole:
        return QString::fromStdString(row.update_source.source_uri);
    case UpdateChannelRole:
        return QString::fromStdString(row.update_source.channel);
    case UpdateCheckedAtUtcRole:
        return QString::fromStdString(row.update_source.checked_at_utc);
    case UpdateMetadataRevisionRole:
        return QString::fromStdString(row.update_source.metadata_revision);
    case UpdateFreshnessRole:
        return toString(row.update_source.freshness);
    case UpdateMetadataCachedRole:
        return row.update_source.metadata_cached
            ? QVariant(*row.update_source.metadata_cached) : QVariant{};
    case UpdateRemoteRefRole:
        return QString::fromStdString(row.update_source.remote_ref);
    case UpdateInstallationScopeRole:
        return QString::fromStdString(row.update_source.installation_scope);
    case UpdateInstallationRootRole:
        return QString::fromStdString(row.update_source.installation_root);
    default:
        return {};
    }
}

QHash<int, QByteArray> QtSoftwareListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SoftwareIdRole] = "softwareId";
    roles[InstanceIdRole] = "instanceId";
    roles[InstalledVersionRole] = "installedVersion";
    roles[CandidateVersionRole] = "candidateVersion";
    roles[UpdateStatusRole] = "updateStatus";
    roles[VersionRelationRole] = "versionRelation";
    roles[CandidateTrustRole] = "candidateTrust";
    roles[CandidateRiskRole] = "candidateRisk";
    roles[IconNameRole] = "iconName";
    roles[InstalledSourceRevisionRole] = "installedSourceRevision";
    roles[InstalledSourceUriRole] = "installedSourceUri";
    roles[UpdateSourceRole] = "updateSource";
    roles[UpdateSourceUriRole] = "updateSourceUri";
    roles[UpdateChannelRole] = "updateChannel";
    roles[UpdateCheckedAtUtcRole] = "updateCheckedAtUtc";
    roles[UpdateMetadataRevisionRole] = "updateMetadataRevision";
    roles[UpdateFreshnessRole] = "updateFreshness";
    roles[UpdateMetadataCachedRole] = "updateMetadataCached";
    roles[UpdateRemoteRefRole] = "updateRemoteRef";
    roles[UpdateInstallationScopeRole] = "updateInstallationScope";
    roles[UpdateInstallationRootRole] = "updateInstallationRoot";
    return roles;
}

QString QtSoftwareListModel::toString(UpdateStatus status)
{
    switch (status) {
    case UpdateStatus::unknown:
        return QStringLiteral("unknown");
    case UpdateStatus::candidate:
        return QStringLiteral("candidate");
    case UpdateStatus::no_candidate:
        return QStringLiteral("no_candidate");
    case UpdateStatus::observed_revision_matches:
        return QStringLiteral("observed_revision_matches");
    case UpdateStatus::up_to_date:
        return QStringLiteral("up_to_date");
    case UpdateStatus::unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unknown");
}

QString QtSoftwareListModel::toString(VersionRelation relation)
{
    switch (relation) {
    case VersionRelation::unknown:
        return QStringLiteral("unknown");
    case VersionRelation::less:
        return QStringLiteral("less");
    case VersionRelation::equal:
        return QStringLiteral("equal");
    case VersionRelation::greater:
        return QStringLiteral("greater");
    case VersionRelation::incomparable:
        return QStringLiteral("incomparable");
    }
    return QStringLiteral("unknown");
}

QString QtSoftwareListModel::toString(TrustSource source)
{
    switch (source) {
    case TrustSource::unknown:
        return QStringLiteral("unknown");
    case TrustSource::official:
        return QStringLiteral("official");
    case TrustSource::vendor:
        return QStringLiteral("vendor");
    case TrustSource::community:
        return QStringLiteral("community");
    case TrustSource::local:
        return QStringLiteral("local");
    }
    return QStringLiteral("unknown");
}

QString QtSoftwareListModel::toString(UpdateRisk risk)
{
    switch (risk) {
    case UpdateRisk::unknown:
        return QStringLiteral("unknown");
    case UpdateRisk::low:
        return QStringLiteral("low");
    case UpdateRisk::medium:
        return QStringLiteral("medium");
    case UpdateRisk::high:
        return QStringLiteral("high");
    case UpdateRisk::critical:
        return QStringLiteral("critical");
    }
    return QStringLiteral("unknown");
}

QString QtSoftwareListModel::toString(MetadataFreshness freshness)
{
    switch (freshness) {
    case MetadataFreshness::unknown:
        return QStringLiteral("unknown");
    case MetadataFreshness::fresh:
        return QStringLiteral("fresh");
    case MetadataFreshness::stale:
        return QStringLiteral("stale");
    }
    return QStringLiteral("unknown");
}

} // namespace ecosystem::core
