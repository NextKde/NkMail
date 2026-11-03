#pragma once
#include "TaskRunner.h"
#include "mail/MailTransport.h"
#include <QObject>
#include <QPointer>
namespace nkmail {
// Checks credentials without storing an account or sending mail.
class AccountSetup final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
public:
    AccountSetup(std::shared_ptr<MailTransport> transport, bool enabled, QObject *parent = nullptr)
        : QObject(parent), m_transport(std::move(transport)), m_enabled(enabled) {}
    ~AccountSetup() override { m_tasks.finish(); }
    bool busy() const { return m_busy; }
    QString stage() const { return m_stage; }
    Q_INVOKABLE void verify(const QVariantMap &config, const QString &password) {
        if (m_busy || !m_enabled) return;
        Account account = accountFromMap(config); account.password = password;
        m_busy = true; m_stage = "imap"; emit changed();
        QPointer<AccountSetup> self(this);
        m_tasks.run([self, account, transport = m_transport] {
            QString error;
            try {
                transport->folders(account);
                if (self) QMetaObject::invokeMethod(self, [self] {
                    if (self) { self->m_stage = "smtp"; emit self->changed(); }
                }, Qt::QueuedConnection);
                transport->verifySmtp(account);
            } catch (...) { error = tr("Connection failed. Check the server settings, application password and trusted certificate."); }
            if (self) QMetaObject::invokeMethod(self, [self, error] {
                if (!self) return;
                self->m_busy = false; self->m_stage = error.isEmpty() ? "ready" : "failed";
                emit self->changed(); emit self->finished(error.isEmpty(), error);
            }, Qt::QueuedConnection);
        });
    }
signals:
    void changed();
    void finished(bool success, const QString &error);
private:
    std::shared_ptr<MailTransport> m_transport;
    bool m_enabled, m_busy = false;
    QString m_stage;
    TaskRunner m_tasks;
};
}
