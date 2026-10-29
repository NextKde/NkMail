#pragma once
#include <QObject>
#include <QString>

namespace nkmail {
// Asynchronous QtKeychain only. Controllers may retain a session-only secret after
// an error; this class never writes an insecure fallback or probes on construction.
class CredentialStore final : public QObject {
    Q_OBJECT
public:
    explicit CredentialStore(QObject *parent = nullptr);
    explicit CredentialStore(const QString &service, QObject *parent = nullptr);
    Q_INVOKABLE void read(const QString &accountId);
    Q_INVOKABLE void write(const QString &accountId, const QString &password);
    Q_INVOKABLE void remove(const QString &accountId);
signals:
    void passwordReady(const QString &accountId, const QString &password);
    void passwordSaved(const QString &accountId);
    void passwordDeleted(const QString &accountId);
    void error(const QString &accountId, const QString &message);
private:
    QString m_service;
};
}
