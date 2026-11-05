#include "AggregateCommands.h"
#include "AccountSession.h"
#include "storage/MailDatabase.h"
#include <QPointer>
#include <QTimer>
namespace nkmail {
AggregateCommands::AggregateCommands(QString root, AccountSession *credentials, std::shared_ptr<MailTransport> transport, QObject *parent)
    : QObject(parent), m_root(std::move(root)), m_credentials(credentials), m_transport(std::move(transport)),
      m_database(std::make_unique<MailDatabase>()), m_commands(new MailboxCommands(*m_database, m_transport, this)) {
    connect(m_commands, &MailboxCommands::completed, this, [this](const QList<Summary> &messages, bool, const QString &error) {
        m_successful.append(messages);
        if (!error.isEmpty()) fail(error);
        advance();
    });
}
AggregateCommands::~AggregateCommands() { delete m_commands; }
void AggregateCommands::execute(const QList<Summary> &messages, MailboxCommands::Action action, bool value, const QString &destination) {
    if (m_busy || messages.isEmpty()) return;
    m_action = action; m_value = value; m_destination = destination;
    m_errors.clear(); m_groups.clear(); m_successful.clear(); m_busy = true;
    for (const auto &s : messages) {
        bool grouped = false;
        for (auto &group : m_groups) {
            const auto &first = group.first();
            if (first.accountId == s.accountId && first.folder == s.folder && first.uidValidity == s.uidValidity) {
                group.append(s); grouped = true; break;
            }
        }
        if (!grouped) m_groups.append(QList<Summary>{s});
    }
    next();
}
void AggregateCommands::fail(const QString &message) {
    m_errors.append(QStringLiteral("%1: %2").arg(m_account.name.isEmpty() ? m_account.address : m_account.name, message));
}
void AggregateCommands::advance() { QTimer::singleShot(0, this, &AggregateCommands::next); }
void AggregateCommands::next() {
    if (m_groups.isEmpty()) {
        m_busy = false;
        emit completed(m_successful, m_action == MailboxCommands::Action::Remove || m_action == MailboxCommands::Action::Move || m_action == MailboxCommands::Action::Archive, m_errors.join(u'\n'));
        return;
    }
    const auto messages = m_groups.takeFirst(); const auto first = messages.first();
    if (first.accountId.isEmpty() || !m_database->open(m_root, first.accountId)) {
        m_errors.append(tr("Message source account is unavailable.")); advance(); return;
    }
    bool found = false;
    for (const auto &account : m_database->accounts()) if (account.id == first.accountId) { m_account = account; found = true; break; }
    if (!found) { m_errors.append(tr("Message source account is unavailable.")); advance(); return; }
    QString destination = m_destination;
    auto action = m_action;
    if (action == MailboxCommands::Action::Archive) {
        destination.clear(); action = MailboxCommands::Action::Move;
        for (const auto &f : m_database->folders())
            if (f.role == "archive" || f.role == "all" || f.id.compare("Archive", Qt::CaseInsensitive) == 0 || f.id.compare("Archives", Qt::CaseInsensitive) == 0 || f.id.compare("All Mail", Qt::CaseInsensitive) == 0) { destination = f.id; break; }
    }
    if (action == MailboxCommands::Action::Move) {
        bool valid = false;
        for (const auto &f : m_database->folders()) if (f.id == destination && f.id != first.folder) valid = true;
        if (m_account.incomingProtocol == "pop3" || !valid) { fail(tr("Destination unavailable for this account. Open its folder and refresh before retrying.")); advance(); return; }
    }
    QPointer<AggregateCommands> self(this);
    const auto account = m_account;
    m_credentials->resolve(account, [self, account, messages, action, destination](const Account &authenticated) {
        if (!self) return;
        if (authenticated.password.isEmpty() && authenticated.incomingProtocol != "pop3") {
            self->fail(tr("Account needs a password. Open account settings to reconnect.")); self->advance(); return;
        }
        // Resolve can be asynchronous. Never send to an edited/deleted account.
        bool matches = false;
        for (const auto &current : self->m_database->accounts())
            if (current.id == account.id && accountMap(current) == accountMap(account)) matches = true;
        if (!matches) { self->fail(tr("Message source account changed.")); self->advance(); return; }
        self->m_commands->execute(authenticated, messages, action, self->m_value, destination);
    });
}
}
