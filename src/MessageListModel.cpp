#include "MessageListModel.h"
#include <QHash>
#include <QRegularExpression>
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
    case ThreadKeyRole: return m_threadKeys.at(index.row());
    case ThreadCountRole: return m_threadCounts.at(index.row());
    case ThreadFirstRole: return m_threadFirst.at(index.row());
    default: return {};
    }
}
QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {{UidRole, "uid"}, {UidValidityRole, "uidValidity"}, {FolderRole, "folder"},
            {SenderRole, "sender"}, {SenderAddressRole, "senderAddress"},
            {SubjectRole, "subject"}, {DateRole, "date"}, {PreviewRole, "preview"},
            {UnreadRole, "unread"}, {FlaggedRole, "flagged"},
            {HasAttachmentsRole, "hasAttachments"}, {ThreadKeyRole, "threadKey"},
            {ThreadCountRole, "threadCount"}, {ThreadFirstRole, "threadFirst"}};
}
QString MessageListModel::normalizedSubject(const QString &subject)
{
    static const QRegularExpression prefix(
        QStringLiteral("^\\s*(?:re\\[\\s*\\d+\\s*\\]|re\\(\\s*\\d+\\s*\\)|re|aw|sv|答复|回复|转发|fwd|fw)\\s*:\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    QString text = subject;
    forever {
        const auto match = prefix.match(text);
        if (!match.hasMatch() || match.capturedLength() == 0)
            break;
        text = text.mid(match.capturedEnd());
    }
    return text.trimmed().toCaseFolded();
}
QString MessageListModel::threadKeyFor(const Summary &s)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    for (const auto &token : s.references.split(whitespace, Qt::SkipEmptyParts)) {
        if (!token.isEmpty())
            return token;
    }
    for (const auto &token : s.inReplyTo.split(whitespace, Qt::SkipEmptyParts)) {
        if (!token.isEmpty())
            return token;
    }
    return normalizedSubject(s.subject);
}
void MessageListModel::rebuildThreads()
{
    const int size = int(m_messages.size());
    m_threadKeys.resize(size);
    m_threadCounts.fill(0, size);
    m_threadFirst.fill(false, size);
    QHash<QString, int> counts;
    for (int row = 0; row < size; ++row) {
        m_threadKeys[row] = threadKeyFor(m_messages.at(row));
        ++counts[m_threadKeys.at(row)];
    }
    QSet<QString> seen;
    for (int row = 0; row < size; ++row) {
        m_threadCounts[row] = counts.value(m_threadKeys.at(row));
        if (!seen.contains(m_threadKeys.at(row))) {
            m_threadFirst[row] = true;
            seen.insert(m_threadKeys.at(row));
        }
    }
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
        rebuildThreads();
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
        rebuildThreads();
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
            rebuildThreads();
            endRemoveRows();
        }
    }
    for (int row = 0; row < unique.size(); ++row) {
        const auto &s = unique.at(row);
        const int old = indexOf(s.folder, s.uidValidity, s.uid);
        if (old < 0) {
            beginInsertRows({}, row, row);
            m_messages.insert(row, s);
            rebuildThreads();
            endInsertRows();
        } else {
            if (old != row) {
                beginMoveRows({}, old, old, {}, old < row ? row + 1 : row);
                m_messages.move(old, row);
                rebuildThreads();
                endMoveRows();
            }
            update(s);
        }
    }
    if (count() != before)
        emit countChanged();
}
}
