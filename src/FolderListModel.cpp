#include "FolderListModel.h"
#include <QSet>

namespace nkmail {
FolderListModel::FolderListModel(QObject *parent) : QAbstractListModel(parent) {}
int FolderListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}
QVariant FolderListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0 || index.row() >= count())
        return {};
    const auto &f = m_folders.at(index.row());
    switch (role) {
    case IdRole: return f.id;
    case TitleRole: return f.title;
    case TotalRole: return f.total;
    default: return {};
    }
}
QHash<int, QByteArray> FolderListModel::roleNames() const
{
    return {{IdRole, "id"}, {TitleRole, "title"}, {TotalRole, "total"}};
}
int FolderListModel::indexOf(const QString &id) const
{
    for (int row = 0; row < count(); ++row)
        if (m_folders.at(row).id == id)
            return row;
    return -1;
}
QVariantMap FolderListModel::get(int row) const
{
    if (row < 0 || row >= count())
        return {};
    const auto &f = m_folders.at(row);
    return {{"id", f.id}, {"title", f.title}, {"total", f.total}};
}
bool FolderListModel::update(const Folder &folder)
{
    const int row = indexOf(folder.id);
    if (row < 0)
        return false;
    const auto &old = m_folders.at(row);
    if (old.title != folder.title || old.total != folder.total) {
        m_folders[row] = folder;
        emit dataChanged(index(row), index(row));
    }
    return true;
}
void FolderListModel::append(const QList<Folder> &folders)
{
    const int before = count();
    for (const auto &f : folders) {
        if (update(f))
            continue;
        const int row = count();
        beginInsertRows({}, row, row);
        m_folders.append(f);
        endInsertRows();
    }
    if (count() != before)
        emit countChanged();
}
void FolderListModel::replace(const QList<Folder> &folders)
{
    const int before = count();
    QList<Folder> unique;
    QSet<QString> wanted;
    for (const auto &f : folders) {
        if (!wanted.contains(f.id)) {
            unique.append(f);
            wanted.insert(f.id);
        }
    }
    for (int row = count() - 1; row >= 0; --row) {
        if (!wanted.contains(m_folders.at(row).id)) {
            beginRemoveRows({}, row, row);
            m_folders.removeAt(row);
            endRemoveRows();
        }
    }
    for (int row = 0; row < unique.size(); ++row) {
        const auto &f = unique.at(row);
        const int old = indexOf(f.id);
        if (old < 0) {
            beginInsertRows({}, row, row);
            m_folders.insert(row, f);
            endInsertRows();
        } else {
            if (old != row) {
                beginMoveRows({}, old, old, {}, old < row ? row + 1 : row);
                m_folders.move(old, row);
                endMoveRows();
            }
            update(f);
        }
    }
    if (count() != before)
        emit countChanged();
}
}
