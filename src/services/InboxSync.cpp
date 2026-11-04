#include "InboxSync.h"
#include "AccountSession.h"
#include "MailboxSync.h"
#include "storage/MailDatabase.h"
#include <QPointer>
#include <QTimer>
#include <utility>

namespace nkmail {
InboxSync::InboxSync(QString root, AccountSession *credentials, std::shared_ptr<MailTransport> transport, QObject *parent)
    : QObject(parent), m_root(std::move(root)), m_credentials(credentials), m_sync(new MailboxSync(std::move(transport), this)) {}
InboxSync::~InboxSync() { delete m_sync; }
void InboxSync::start(const QList<Account> &accounts) {
    if (m_running || accounts.isEmpty()) return;
    m_running = true; m_pending = accounts; m_failures.clear();
    const auto generation = ++m_generation;
    emit runningChanged();
    next(generation);
}
void InboxSync::cancel() {
    const bool wasRunning = m_running;
    ++m_generation; m_pending.clear(); m_failures.clear(); m_running = false;
    if (wasRunning) emit runningChanged();
    m_sync->resetReadConnection();
}
void InboxSync::failed(const Account &account, const QString &error) {
    m_failures.append(QStringLiteral("%1: %2").arg(account.name.isEmpty() ? account.address : account.name, error));
}
void InboxSync::next(quint64 generation) {
    if (generation != m_generation) return;
    if (m_pending.isEmpty()) {
        m_running = false; emit runningChanged(); emit finished(m_failures); return;
    }
    const auto requested = m_pending.takeFirst();
    QPointer<InboxSync> self(this);
    m_credentials->resolve(requested, [self, requested, generation](const Account &account) {
        if (!self || generation != self->m_generation) return;
        auto advance = [self, generation] {
            if (self) QTimer::singleShot(0, self, [self, generation] { if (self) self->next(generation); });
        };
        if (account.password.isEmpty()) {
            self->failed(account, tr("Account needs a password.")); advance(); return;
        }
        // Refresh only the newest bounded header page; older cached mail remains
        // available. Bodies are still downloaded on demand through the reader.
        self->m_sync->page(account, QStringLiteral("INBOX"), 0, 50,
            [self, requested, generation, advance](const Page &page, const QString &error) {
                if (!self || generation != self->m_generation) return;
                if (!error.isEmpty()) { self->failed(requested, error); advance(); return; }
                MailDatabase db;
                if (!db.open(self->m_root, requested.id)) { self->failed(requested, db.error()); advance(); return; }
                bool matches = false;
                for (const auto &current : db.accounts())
                    if (current.id == requested.id && accountMap(current) == accountMap(requested)) { matches = true; break; }
                // Deleted/edited accounts cannot receive results from the old endpoint.
                if (!matches) { advance(); return; }
                if (page.folder != QLatin1String("INBOX") || page.uidValidity.isEmpty()) {
                    self->failed(requested, tr("Invalid inbox response.")); advance(); return;
                }
                const auto reconciled = db.reconcilePopPage(page);
                if (!db.savePage(reconciled)) self->failed(requested, db.error());
                else emit self->updated(requested.id, page.uidValidity);
                advance();
            });
    });
}
}
