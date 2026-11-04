#pragma once
#include "MailTypes.h"
#include <QObject>
#include <QStringList>
#include <memory>

namespace nkmail {
class AccountSession;
class MailboxSync;
class MailTransport;
// Independent, serial inbox refreshes. Browsing another account never rebinds
// the cache destination of an in-flight request.
class InboxSync final : public QObject {
    Q_OBJECT
public:
    InboxSync(QString root, AccountSession *credentials, std::shared_ptr<MailTransport> transport, QObject *parent = nullptr);
    ~InboxSync() override;
    bool running() const { return m_running; }
    void start(const QList<Account> &accounts);
    void cancel();
signals:
    void runningChanged();
    void updated(const QString &accountId, const QString &validity);
    void finished(const QStringList &failures);
private:
    void next(quint64 generation);
    void failed(const Account &account, const QString &error);
    QString m_root;
    AccountSession *m_credentials;
    MailboxSync *m_sync;
    QList<Account> m_pending;
    QStringList m_failures;
    quint64 m_generation = 0;
    bool m_running = false;
};
}
