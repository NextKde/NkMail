#include "MailboxCommands.h"
#include "mail/MailTransport.h"
#include "storage/MailDatabase.h"
#include <QPointer>
namespace nkmail {
MailboxCommands::MailboxCommands(MailDatabase &database, std::shared_ptr<MailTransport> transport, QObject *parent)
    : QObject(parent), m_database(database), m_transport(std::move(transport)) {}
MailboxCommands::~MailboxCommands() { m_tasks.finish(); }
void MailboxCommands::execute(const Account &account, QList<Summary> messages, Action action,
                              bool value, const QString &destination, bool demo) {
    if (m_busy || messages.isEmpty()) return;
    const auto first = messages.first();
    if (account.id != m_database.accountId()) {
        emit completed({}, false, tr("The command belongs to another account.")); return;
    }
    if (action == Action::Move && (destination.trimmed().isEmpty() || destination == first.folder)) {
        emit completed({}, false, tr("Choose a different destination folder.")); return;
    }
    QStringList uids;
    for (auto &message : messages) {
        if (message.folder != first.folder || message.uidValidity != first.uidValidity
            || message.folder.isEmpty() || message.uidValidity.isEmpty() || message.uid.isEmpty()) {
            emit completed({}, false, tr("Select messages from a single valid mailbox.")); return;
        }
        if (!uids.contains(message.uid)) uids.append(message.uid);
        if (action == Action::Read) message.unread = !value;
        if (action == Action::Flag) message.flagged = value;
    }
    const bool removed = action == Action::Remove || action == Action::Move;
    m_busy = true;
    QPointer<MailboxCommands> self(this);
    auto finish = [self, account, messages, removed](const QString &error) {
        if (!self) return;
        self->m_busy = false;
        if (!error.isEmpty()) { emit self->completed({}, false, error); return; }
        if (self->m_database.accountId() != account.id) {
            emit self->completed({}, false, tr("Account changed before the result could be cached.")); return;
        }
        bool cached = true;
        if (removed) {
            for (const auto &s : messages)
                cached = self->m_database.removeMessage(s.folder, s.uidValidity, s.uid) && cached;
        } else cached = self->m_database.updateFlags(messages);
        emit self->completed(messages, removed, cached ? QString{} : tr("Server updated, but caching failed. Refresh before continuing."));
    };
    if (demo) { finish({}); return; }
    m_tasks.run([self, transport = m_transport, account, first, uids, action, value, destination, finish] {
        QString error;
        try {
            if (action == Action::Read || action == Action::Flag)
                transport->storeFlagsMany(account, first.folder, first.uidValidity, uids,
                    {action == Action::Read ? QStringLiteral("\\Seen") : QStringLiteral("\\Flagged")}, value);
            else transport->removeOrMove(account, first.folder, first.uidValidity, uids, action == Action::Move ? destination : QString{});
        } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); }
        catch (...) { error = tr("Mailbox operation failed. Refresh before retrying."); }
        if (self) QMetaObject::invokeMethod(self, [finish, error] { finish(error); }, Qt::QueuedConnection);
    });
}
}
