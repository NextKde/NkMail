#pragma once
#include "TaskRunner.h"
#include "mail/MailTransport.h"
#include "auth/GoogleOAuth.h"
#include <QObject>
#include <QPointer>
namespace nkmail {
// Checks credentials without storing an account or sending mail.
class AccountSetup final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(bool googleConfigured READ googleConfigured CONSTANT)
    Q_PROPERTY(bool googleAuthorized READ googleAuthorized NOTIFY changed)
    Q_PROPERTY(QVariantMap googleAccount READ googleAccount NOTIFY changed)
public:
    AccountSetup(std::shared_ptr<MailTransport> transport, bool enabled, QObject *parent = nullptr)
        : QObject(parent), m_transport(std::move(transport)), m_enabled(enabled), m_google(new GoogleOAuthFlow(this)) {
        connect(m_google, &GoogleOAuthFlow::failed, this, [this](const QString &error) {
            m_busy = false; m_stage = "failed"; emit changed(); emit finished(false, error);
        });
        connect(m_google, &GoogleOAuthFlow::granted, this, [this](const QString &email, const QString &credential) {
            Account account; account.id = email.toLower(); account.address = account.username = email;
            account.name = email; account.password = credential; account.authMethod = "google_oauth";
            account.imapHost = "imap.gmail.com"; account.smtpHost = "smtp.gmail.com";
            account.serverSavesSent = true;
            // Google's token/verified-identity exchange completes sign-in. Mail
            // connectivity is handled by the normal sync lifecycle, so a port
            // outage does not throw away a valid authorization or force re-consent.
            m_googleAccount = account; m_googleAuthorized = true;
            m_busy = false; m_stage = "ready"; emit changed(); emit finished(true, {});
        });
    }
    ~AccountSetup() override { m_google->cancel(); m_tasks.finish(); GoogleTokens::forget(m_googleAccount.password); }
    bool busy() const { return m_busy; }
    QString stage() const { return m_stage; }
    bool googleConfigured() const { return m_google->configured(); }
    bool googleAuthorized() const { return m_googleAuthorized; }
    QVariantMap googleAccount() const { return accountMap(m_googleAccount); }
    Account authorizedGoogleAccount() const { return m_googleAuthorized && !m_busy ? m_googleAccount : Account{}; }
    Account takeGoogleAccount() {
        if (!m_googleAuthorized || m_busy) return {};
        auto account = m_googleAccount; m_googleAccount = {}; m_googleAuthorized = false;
        emit changed(); return account;
    }
    Q_INVOKABLE void beginGoogleSignIn(const QString &hint = {}) {
        if (m_busy || !m_enabled) return;
        GoogleTokens::forget(m_googleAccount.password); m_googleAccount = {}; m_googleAuthorized = false;
        m_busy = true; m_stage = "google"; emit changed(); m_google->start(hint);
    }
    Q_INVOKABLE void cancelGoogleSignIn() {
        m_google->cancel(); ++m_generation;
        GoogleTokens::forget(m_googleAccount.password); m_googleAccount = {}; m_googleAuthorized = false;
        m_busy = false; m_stage.clear(); emit changed();
    }
    Q_INVOKABLE void verify(const QVariantMap &config, const QString &password) {
        if (m_busy || !m_enabled) return;
        m_googleAuthorized = false;
        Account account = accountFromMap(config); account.authMethod = "password"; account.password = password;
        check(account);
    }
signals:
    void changed();
    void finished(bool success, const QString &error);
private:
    void check(const Account &account) {
        m_busy = true; m_stage = "imap"; emit changed();
        const auto generation = ++m_generation;
        QPointer<AccountSetup> self(this);
        m_tasks.run([self, generation, account, transport = m_transport] {
            QString error;
            try {
                transport->folders(account);
                if (self) QMetaObject::invokeMethod(self, [self, generation] {
                    if (self && self->m_generation == generation) { self->m_stage = "smtp"; emit self->changed(); }
                }, Qt::QueuedConnection);
                transport->verifySmtp(account);
            } catch (...) { error = tr("Connection failed. Check the server settings, application password and trusted certificate."); }
            if (self) QMetaObject::invokeMethod(self, [self, generation, account, error] {
                if (!self || self->m_generation != generation) return;
                self->m_busy = false; self->m_stage = error.isEmpty() ? "ready" : "failed";
                if (error.isEmpty() && account.authMethod == "google_oauth") {
                    self->m_googleAccount = account; self->m_googleAuthorized = true;
                } else if (account.authMethod == "google_oauth") GoogleTokens::forget(account.password);
                emit self->changed(); emit self->finished(error.isEmpty(), error);
            }, Qt::QueuedConnection);
        });
    }
    std::shared_ptr<MailTransport> m_transport;
    bool m_enabled, m_busy = false, m_googleAuthorized = false;
    QString m_stage;
    Account m_googleAccount;
    GoogleOAuthFlow *m_google;
    quint64 m_generation = 0;
    TaskRunner m_tasks;
};
}
