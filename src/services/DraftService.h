#pragma once
#include "MailTypes.h"
#include <QObject>
namespace nkmail {
class MailDatabase;
class DraftService final : public QObject {
    Q_OBJECT
public:
    DraftService(MailDatabase &database, const QString &root, QObject *parent = nullptr)
        : QObject(parent), m_root(root), m_database(&database) {}
    QVariantMap newDraft(const QString &mode, const Account &account, const Content &content);
    QVariantMap loadDraft(const QString &id);
    QVariantMap saveDraft(const QVariantMap &draft);
    void discardDraft(const QString &id);
    QVariantMap addAttachment(const QString &url);
    void saveAttachment(const Content &content, int index, const QString &url);
signals:
    void draftsChanged();
    void status(const QString &message, const QString &error);
private:
    void setStatus(const QString &message, const QString &error = {}) { emit status(message, error); }
    QString m_root;
    MailDatabase *m_database;
};
}
