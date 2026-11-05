#pragma once
#include "MailTypes.h"
#include "TaskRunner.h"
#include <QObject>
#include <memory>
namespace nkmail {
class MailDatabase;
class MailTransport;
class MailboxCommands final : public QObject {
    Q_OBJECT
public:
    enum class Action { Read, Flag, Remove, Move, Archive };
    MailboxCommands(MailDatabase &database, std::shared_ptr<MailTransport> transport, QObject *parent = nullptr);
    ~MailboxCommands() override;
    bool busy() const { return m_busy; }
    void execute(const Account &account, QList<Summary> messages, Action action,
                 bool value = false, const QString &destination = {}, bool demo = false);
signals:
    void completed(const QList<nkmail::Summary> &messages, bool removed, const QString &error);
private:
    MailDatabase &m_database;
    std::shared_ptr<MailTransport> m_transport;
    bool m_busy = false;
    TaskRunner m_tasks;
};
}
