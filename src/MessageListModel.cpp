#include "MessageListModel.h"
#include <QSet>

namespace nkmail {
MessageListModel::MessageListModel(QObject *parent) : QAbstractListModel(parent) {}
int MessageListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}
QVariant MessageListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= count())
        return {};
    const auto &s = m_messages.at(index.row());
    switch (role) {
    case UidRole: return s.uid;
    case UidValidityRole: return s.uidValidity;
    case FolderRole: return s.folder;
    case SenderRole: return s.sender;
    case SenderAddressRole: return s.senderAddress;
    case SubjectRole: return s.subject;
    case DateRole: return s.date;
    case PreviewRole: return s.preview;
    case UnreadRole: return s.unread;
    case FlaggedRole: return s.flagged;
    case HasAttachmentsRole: return s.hasAttachments;
    default: return {};
    }
}
QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {{UidRole, "uid"}, {UidValidityRole, "uidValidity"}, {FolderRole, "folder"},
            {SenderRole, "sender"}, {SenderAddressRole, "senderAddress"},
            {SubjectRole, "subject"}, {DateRole, "date"}, {PreviewRole, "preview"},
            {UnreadRole, "unread"}, {FlaggedRole, "flagged"},
            {HasAttachmentsRole, "hasAttachments"}};
}
QString MessageListModel::identity(const Summary &s)
{
    return QString::number(s.folder.size()) + ':' + s.folder
         + QString::number(s.uidValidity.size()) + ':' + s.uidValidity
         + QString::number(s.uid.size()) + ':' + s.uid;
}
int MessageListModel::indexOf(const QString &folder, const QString &uidValidity,
                               const QString &uid) const
{
    for (int row = 0; row < count(); ++row) {
        const auto &s = m_messages.at(row);
        if (s.folder == folder && s.uidValidity == uidValidity && s.uid == uid)
            return row;
    }
    return -1;
}
QVariantMap MessageListModel::get(int row) const
{
    return row >= 0 && row < count() ? summaryMap(m_messages.at(row)) : QVariantMap{};
}
bool MessageListModel::update(const Summary &message)
{
    const int row = indexOf(message.folder, message.uidValidity, message.uid);
    if (row < 0)
        return false;
    if (summaryMap(m_messages.at(row)) != summaryMap(message)) {
        m_messages[row] = message;
        emit dataChanged(index(row), index(row));
    }
    return true;
}
void MessageListModel::append(const QList<Summary> &messages)
{
    const int before = count();
    for (const auto &s : messages) {
        if (update(s))
            continue;
        const int row = count();
        beginInsertRows({}, row, row);
        m_messages.append(s);
        endInsertRows();
    }
    if (count() != before)
        emit countChanged();
}
void MessageListModel::replace(const QList<Summary> &messages)
{
    const int before = count();
    QList<Summary> unique;
    QSet<QString> wanted;
    for (const auto &s : messages) {
        const QString key = identity(s);
        if (!wanted.contains(key)) {
            unique.append(s);
            wanted.insert(key);
        }
    }
    // Incremental remove/move/insert preserves persistent indexes and selection of
    // surviving (folder, UIDVALIDITY, UID) identities, including reordered pages.
    for (int row = count() - 1; row >= 0; --row) {
        if (!wanted.contains(identity(m_messages.at(row)))) {
            beginRemoveRows({}, row, row);
            m_messages.removeAt(row);
            endRemoveRows();
        }
    }
    for (int row = 0; row < unique.size(); ++row) {
        const auto &s = unique.at(row);
        const int old = indexOf(s.folder, s.uidValidity, s.uid);
        if (old < 0) {
            beginInsertRows({}, row, row);
            m_messages.insert(row, s);
            endInsertRows();
        } else {
            if (old != row) {
                beginMoveRows({}, old, old, {}, old < row ? row + 1 : row);
                m_messages.move(old, row);
                endMoveRows();
            }
            update(s);
        }
    }
    if (count() != before)
        emit countChanged();
}
}
