#pragma once
#include "MailboxCommands.h"
#include <QObject>
#include <memory>
namespace nkmail {
class AccountSession;
class MailDatabase;
class AggregateCommands final : public QObject {
    Q_OBJECT
public:
    AggregateCommands(QString root, AccountSession *credentials, std::shared_ptr<MailTransport> transport, QObject *parent = nullptr);
    ~AggregateCommands() override;
    bool busy() const { return m_busy; }
    void execute(const QList<Summary> &messages, MailboxCommands::Action action, bool value, const QString &destination);
signals:
    void completed(const QList<nkmail::Summary> &messages, bool removed, const QString &error);
private:
    void next();
    void advance();
    void fail(const QString &message);
    QString m_root;
    AccountSession *m_credentials;
    std::shared_ptr<MailTransport> m_transport;
    std::unique_ptr<MailDatabase> m_database;
    MailboxCommands *m_commands;
    QList<QList<Summary>> m_groups;
    QList<Summary> m_successful;
    QStringList m_errors;
    Account m_account;
    MailboxCommands::Action m_action = MailboxCommands::Action::Read;
    QString m_destination;
    bool m_value = false, m_busy = false;
};
}
