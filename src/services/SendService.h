#pragma once
#include "MailTypes.h"
#include "TaskRunner.h"
#include <QObject>
#include <QTimer>
#include <memory>
namespace nkmail {
class MailDatabase;
class MailTransport;
class DraftService;
class SendService final : public QObject {
    Q_OBJECT
public:
    SendService(bool demo, MailDatabase &database, DraftService &drafts,
                std::shared_ptr<MailTransport> transport, QObject *parent = nullptr);
    ~SendService() override;
    bool sending() const { return m_sending; }
    bool pending() const { return m_sendPending; }
    int delay() const { return m_sendDelay; }
    void setDelay(int seconds) { m_sendDelay = qBound(0, seconds, 60); }
    void sendDraft(const Account &account, const QVariantMap &draft);
    void cancelSend();
    void clearPendingSend();
signals:
    void draftsChanged();
    void sendPendingChanged();
    void draftAccepted(const QString &id);
    void status(const QString &message, const QString &error);
private:
    void submitDraft(const Draft &draft);
    void setStatus(const QString &message, const QString &error = {}) { emit status(message, error); }
    bool m_demo, m_sending = false, m_sendPending = false;
    int m_sendDelay = 0;
    Account m_account;
    Draft m_pendingSend;
    QTimer m_sendTimer;
    MailDatabase *m_database;
    DraftService &m_drafts;
    std::shared_ptr<MailTransport> m_transport;
    TaskRunner m_tasks;
};
}
