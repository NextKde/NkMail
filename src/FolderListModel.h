#pragma once
#include "MailTypes.h"
#include <QAbstractListModel>

namespace nkmail {
class FolderListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { IdRole = Qt::UserRole + 1, TitleRole, TotalRole };
    Q_ENUM(Role)
    explicit FolderListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return int(m_folders.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(const QList<Folder> &folders);
    void append(const QList<Folder> &folders);
    bool update(const Folder &folder);
    void clear() { replace({}); }
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOf(const QString &id) const;
signals:
    void countChanged();
private:
    QList<Folder> m_folders;
};
}
