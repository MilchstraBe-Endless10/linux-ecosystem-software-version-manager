#pragma once

#include "core_model.hpp"

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QVariant>
#include <vector>

namespace ecosystem::core {

class QtSoftwareListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SoftwareIdRole = Qt::UserRole + 1,
        InstanceIdRole,
        InstalledVersionRole,
        CandidateVersionRole,
        UpdateStatusRole,
        VersionRelationRole,
        CandidateTrustRole,
        CandidateRiskRole,
        IconNameRole,
        InstalledSourceRevisionRole,
        InstalledSourceUriRole,
        UpdateSourceRole,
        UpdateSourceUriRole,
        UpdateChannelRole,
        UpdateCheckedAtUtcRole,
        UpdateMetadataRevisionRole,
        UpdateFreshnessRole,
        UpdateMetadataCachedRole,
        UpdateRemoteRefRole,
        UpdateInstallationScopeRole,
        UpdateInstallationRootRole
    };

    explicit QtSoftwareListModel(QObject *parent = nullptr);

    void setRows(const std::vector<SoftwareRowDto> &rows);
    void setSnapshot(const CoreSnapshot &snapshot);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    static QString toString(UpdateStatus status);
    static QString toString(VersionRelation relation);
    static QString toString(TrustSource source);
    static QString toString(UpdateRisk risk);
    static QString toString(MetadataFreshness freshness);

    std::vector<SoftwareRowDto> rows_;
};

} // namespace ecosystem::core
