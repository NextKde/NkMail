#pragma once
#include "MailTypes.h"
#include <QObject>
#include <QHash>
#include <functional>
namespace nkmail {
class CredentialStore;
// Owns credential keys and session-only secrets, including background accounts.
class AccountSession final : public QObject {
    Q_OBJECT
public:
    explicit AccountSession(const QString &root, QObject *parent = nullptr);
    QString key(const Account &account) const;
    void read(const QString &key);
    void write(const QString &key, const QString &password);
    void remove(const QString &key);
    void setPassword(const Account &account);
    void clear();
    void suspend(const Account &account);
    void forget(const Account &account);
    void resolve(const Account &account, std::function<void(Account)> completion);
signals:
    void passwordReady(const QString &key, const QString &password);
    void passwordSaved(const QString &key);
    void error(const QString &key, const QString &message);
private:
    QString m_root;
    CredentialStore *m_store;
    QHash<QString, QString> m_passwords;
    QHash<QString, QList<std::function<void(QString)>>> m_waiters;
};
}
