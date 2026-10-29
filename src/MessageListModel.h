#pragma once
#include "MailTypes.h"
#include <QAbstractListModel>

namespace nkmail {
class MessageListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role { UidRole = Qt::UserRole + 1, UidValidityRole, FolderRole, SenderRole,
                SenderAddressRole, SubjectRole, DateRole, PreviewRole, UnreadRole,
                FlaggedRole, HasAttachmentsRole };
    Q_ENUM(Role)
    explicit MessageListModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return int(m_messages.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(const QList<Summary> &messages);
    void append(const QList<Summary> &messages);
    bool update(const Summary &message);
    void clear() { replace({}); }
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOf(const QString &folder, const QString &uidValidity,
                            const QString &uid) const;
    static QString identity(const Summary &message);
signals:
    void countChanged();
private:
    QList<Summary> m_messages;
};
}
