#include "MailController.h"
#include "MessageListModel.h"
#include "NotificationService.h"
#include "storage/MailDatabase.h"
#include "storage/CredentialStore.h"
#include "mail/backend.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>

namespace nkmail {
namespace {
constexpr int pageSize = 50;
constexpr qint64 attachmentLimit = 25 * 1024 * 1024;
QString localPath(const QString &url) {
    const QUrl value(url);
    return value.isLocalFile() ? value.toLocalFile() : QString{};
}
QString cleanSubject(const QString &prefix, const QString &subject) {
    return subject.startsWith(prefix, Qt::CaseInsensitive) ? subject : prefix + subject;
}
QString escapeHtml(const QString &text)
{
    QString out; out.reserve(text.size());
    for (const auto ch : text) {
        switch (ch.unicode()) {
        case u'&': out += QStringLiteral("&amp;"); break;
        case u'<': out += QStringLiteral("&lt;"); break;
        case u'>': out += QStringLiteral("&gt;"); break;
        case u'"': out += QStringLiteral("&quot;"); break;
        default: out += ch;
        }
    }
    return out;
}
// Deliberately bounded mailbox-list subset for automatic reply generation. We
// extract addr-specs, never display-name fragments. Unsupported RFC groups,
// comments, quoted local parts and malformed input fail closed for manual review.
bool replyAddresses(const QString &field, QStringList *addresses) {
    addresses->clear();
    if (field.trimmed().isEmpty()) return true;
    QStringList tokens;
    QString token;
    bool quoted = false, escaped = false, angle = false;
    for (const QChar ch : field) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7f) return false;
        if (quoted) {
            token += ch;
            if (escaped) escaped = false;
            else if (ch == QLatin1Char('\\')) escaped = true;
            else if (ch == QLatin1Char('"')) quoted = false;
            continue;
        }
        if (ch == QLatin1Char('"')) quoted = true;
        else if (ch == QLatin1Char('<')) { if (angle) return false; angle = true; }
        else if (ch == QLatin1Char('>')) { if (!angle) return false; angle = false; }
        else if (ch == QLatin1Char(',') && !angle) {
            if (token.trimmed().isEmpty()) return false;
            tokens.append(token.trimmed()); token.clear(); continue;
        }
        else if (ch == QLatin1Char(':') || ch == QLatin1Char(';')
                 || ch == QLatin1Char('(') || ch == QLatin1Char(')') || ch == QLatin1Char('\\')) return false;
        token += ch;
    }
    if (quoted || escaped || angle || token.trimmed().isEmpty()) return false;
    tokens.append(token.trimmed());
    static const QRegularExpression atom(QStringLiteral("^[A-Za-z0-9!#$%&'*+/=?^_`{|}~-]+$"));
    static const QRegularExpression label(QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?$"));
    for (const auto &value : tokens) {
        QString address = value;
        const int opening = value.indexOf('<');
        if (opening >= 0) {
            if (!value.endsWith('>') || value.count('<') != 1 || value.count('>') != 1) return false;
            address = value.mid(opening + 1, value.size() - opening - 2).trimmed();
        } else if (value.contains('>')) return false;
        if (address.size() > 254 || address.count('@') != 1) return false;
        const auto parts = address.split('@');
        if (parts.first().isEmpty() || parts.first().size() > 64 || parts.last().isEmpty()) return false;
        for (const auto &part : parts.first().split('.'))
            if (!atom.match(part).hasMatch()) return false;
        for (const auto &part : parts.last().split('.'))
            if (part.size() > 63 || !label.match(part).hasMatch()) return false;
        addresses->append(address);
    }
    return true;
}
Summary summaryFromMap(const QVariantMap &map) {
    Summary summary;
    summary.folder = map.value("folder").toString();
    summary.uidValidity = map.value("uidValidity").toString();
    summary.uid = map.value("uid").toString();
    summary.sender = map.value("sender").toString();
    summary.senderAddress = map.value("senderAddress").toString();
    summary.recipients = map.value("recipients").toString();
    summary.subject = map.value("subject").toString();
    summary.date = map.value("date").toString();
    summary.preview = map.value("preview").toString();
    summary.messageId = map.value("messageId").toString();
    summary.inReplyTo = map.value("inReplyTo").toString();
    summary.references = map.value("references").toString();
    summary.unread = map.value("unread").toBool();
    summary.flagged = map.value("flagged").toBool();
    summary.hasAttachments = map.value("hasAttachments").toBool();
    return summary;
}
QList<Summary> modelSnapshot(const MessageListModel *model) {
    QList<Summary> result;
    for (int row = 0; row < model->count(); ++row) result.append(summaryFromMap(model->get(row)));
    return result;
}
bool sameMessage(const Summary &a, const Summary &b) {
    return a.folder == b.folder && a.uidValidity == b.uidValidity && a.uid == b.uid;
}
bool isArchiveFolder(const QVariantList &folders, const QString &destination) {
    const QStringList names{QStringLiteral("Archive"), QStringLiteral("Archives"), QStringLiteral("All Mail")};
    for (const auto &value : folders) {
        const auto folder = value.toMap();
        const auto id = folder.value("id").toString();
        if (id != destination) continue;
        const auto title = folder.value("title").toString();
        for (const auto &name : names)
            if (title.compare(name, Qt::CaseInsensitive) == 0 || id.compare(name, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}
}
MailController::MailController(bool demo, const QString &dataRoot, QObject *parent)
    : QObject(parent), m_demo(demo), m_root(dataRoot), m_database(std::make_unique<MailDatabase>()),
      m_messages(new MessageListModel(this)), m_credentials(new CredentialStore(this)) {
    m_pool.setMaxThreadCount(1);
    m_pool.setExpiryTimeout(-1);
    m_refreshTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] {
        if (m_busy || m_sending || m_demo || !configured() || m_account.password.isEmpty()) return;
        fetchPage(false, true);
    });
    connect(m_credentials, &CredentialStore::passwordReady, this, [this](const QString &id, const QString &password) {
        if (id != credentialKey() || m_demo || !m_restorePending || !m_account.password.isEmpty()) return;
        m_restorePending = false;
        m_switchCredentialPending = false;
        m_account.password = password;
        setStatus(tr("Saved credentials restored. Refresh to connect."));
    });
    connect(m_credentials, &CredentialStore::passwordSaved, this, [this](const QString &) {
        QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
        if (!settings.value("restoreAllowed", false).toBool()) return;
        if (!m_busy && !m_sending) setStatus(tr("Credentials saved in the system keychain."));
    });
    connect(m_credentials, &CredentialStore::error, this, [this](const QString &id, const QString &) {
        if (id != credentialKey()) return;
        m_restorePending = false;
        if (m_switchCredentialPending) {
            // A switch could not restore a stored secret (most often no entry).
            // Stay on the newly selected account and ask for its password.
            m_switchCredentialPending = false;
            m_account.password.clear();
            setStatus(tr("Account needs a password"), tr("Enter an application password for this account."));
            return;
        }
        setStatus(tr("System keychain needs attention"), tr("Credential storage or removal could not be completed. The local restore permission is authoritative; no plaintext fallback is used."));
    });
    QTimer::singleShot(0, this, &MailController::initialize);
}
MailController::~MailController() { m_pool.clear(); m_pool.waitForDone(); }
QObject *MailController::messages() const { return m_messages; }
bool MailController::configured() const { return !m_account.address.isEmpty() && !m_account.imapHost.isEmpty(); }
QVariantMap MailController::account() const { return accountMap(m_account); }
QVariantList MailController::accounts() const {
    QVariantList out;
    if (!m_database || !m_database->isOpen()) return out;
    for (const auto &account : m_database->accounts()) out.append(accountMap(account));
    return out;
}
QVariantList MailController::drafts() const {
    QVariantList out;
    if (m_database->isOpen()) for (const auto &draft : m_database->drafts()) out.append(draftMap(draft));
    return out;
}
void MailController::setStatus(const QString &status, const QString &error) {
    m_status = status; m_error = error; emit statusChanged();
}
QString MailController::credentialKey() const { return credentialKey(m_account); }
QString MailController::credentialKey(const Account &account) const {
    // Keep the canonical-root hash used for profile scoping and prefix it with the
    // account id. The same username configured in two accounts of one profile now
    // maps to distinct keys, while separate profiles never share a secret.
    const QFileInfo root(m_root);
    QString path = root.canonicalFilePath();
    if (path.isEmpty()) path = root.absoluteFilePath();
    const QByteArray scope = path.toUtf8() + '\0' + account.id.toUtf8();
    const QString profile = QStringLiteral("profile-") + QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
    return account.id + QLatin1Char(':') + profile;
}
void MailController::run(std::function<void()> task) { (void)QtConcurrent::run(&m_pool, std::move(task)); }
void MailController::initialize() {
    if (!m_database->open(m_root, m_account.id)) { setStatus(tr("Storage unavailable"), m_database->error()); return; }
    if (m_demo) { seedDemo(); return; }
    m_account = m_database->account();
    for (const auto &folder : m_database->folders()) m_folders.append(QVariantMap{{"id", folder.id}, {"title", folder.title}, {"total", folder.total}});
    emit accountChanged(); emit foldersChanged(); emit draftsChanged();
    loadCached();
    setStatus(configured() ? tr("Offline · cached mail") : tr("Add your email account to get started"));
    startRefreshTimer();
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    if (configured() && settings.value("restoreAllowed", false).toBool()) {
        m_restorePending = true;
        m_credentials->read(credentialKey());
    }
}
void MailController::seedDemo() {
    m_account.name = QStringLiteral("Design Studio"); m_account.address = QStringLiteral("you@example.invalid");
    m_account.username = m_account.address; m_account.imapHost = QStringLiteral("imap.example.invalid");
    m_account.smtpHost = QStringLiteral("smtp.example.invalid");
    const QList<Folder> folders{{"INBOX", "Inbox", 8}, {"Sent", "Sent", 1}, {"Archive", "Archive", 0}};
    m_database->saveFolders(folders);
    for (const auto &f : folders) m_folders.append(QVariantMap{{"id", f.id}, {"title", f.title}, {"total", f.total}});
    const QStringList senders{"Maya Chen", "NextKde Design", "Oliver Martin", "Studio Notes", "Lin Wei", "Alex Rivera", "Open Source Weekly", "NkMail"};
    const QStringList subjects{"A quieter place for your inbox", "NextKde · the details make the difference", "Friday's design review", "A few things worth keeping", "关于下一次设计讨论", "Re: The little things", "This week in open source", "Welcome to NkMail"};
    const QStringList previews{"A first look at a more thoughtful mail experience.", "A small update on components, spacing and the next steps.", "I've added a few notes ahead of our conversation.", "References, ideas, and some room to think.", "整理了一些界面与交互建议，期待你的想法。", "Thank you for the careful feedback. Here's the revised direction.", "Good projects, thoughtful tools, and community updates.", "Your mail, with a little more breathing room."};
    Page page; page.folder = "INBOX"; page.uidValidity = "20261029";
    for (int i = 0; i < senders.size(); ++i) {
        Summary s; s.folder = page.folder; s.uidValidity = page.uidValidity; s.uid = QString::number(100 - i);
        s.sender = senders[i]; s.senderAddress = QStringLiteral("hello%1@example.invalid").arg(i + 1);
        s.recipients = m_account.address; s.subject = subjects[i]; s.preview = previews[i];
        s.date = i < 3 ? QStringLiteral("%1:%2").arg(10 - i).arg(42 - i * 7, 2, 10, QLatin1Char('0')) : QStringLiteral("Yesterday");
        s.unread = i < 2; s.flagged = i == 2; s.hasAttachments = i == 0;
        page.messages.append(s);
        Content c; c.summary = s; c.to = m_account.address; c.replyTo = s.senderAddress;
        c.messageId = QStringLiteral("<demo-%1@example.invalid>").arg(i);
        c.text = i == 0 ? QStringLiteral("Hi there,\n\nYour inbox should feel like a place to think, not a place to catch up.\n\nWe've been exploring a quieter way to read and write email — one that puts your conversations first and lets everything else settle into the background.\n\nA few details we think you'll appreciate:\n\n  •  A clear, comfortable reading space\n  •  Familiar controls, right where you expect them\n  •  Drafts that stay with you\n  •  Your mail, kept on your device\n\nI've attached a short note with the direction. Take a look whenever you have a moment.\n\nTalk soon,\nMaya\n\n—\nThis is synthetic demonstration content. No real mailbox is connected.")
                      : previews[i] + QStringLiteral("\n\nThis message is part of NkMail's synthetic preview.\nNo external messages are sent from demonstration mode.\n\nBest,\n") + senders[i];
        if (i == 0) { Attachment a; a.name = "Design direction.txt"; a.mimeType = "text/plain"; a.bytes = "A calmer inbox. Clear hierarchy. Familiar interactions.\n"; a.size = a.bytes.size(); c.attachments.append(a); }
        m_database->savePage(Page{s.folder, s.uidValidity, {s}, false, 0}); m_database->saveContent(c);
    }
    m_database->savePage(page);
    emit accountChanged(); emit foldersChanged(); emit draftsChanged();
    loadCached();
    setStatus(tr("Preview · synthetic mail · not connected"));
    if (!page.messages.isEmpty()) openMessage(page.messages.first().uid, page.uidValidity);
}
void MailController::configureAccount(const QVariantMap &configuration, const QString &password, bool remember) {
    if (m_demo) { setStatus(tr("Preview mode"), tr("Account changes are disabled in this synthetic preview.")); return; }
    if (m_busy || m_sending) return;
    Account next = accountFromMap(configuration);
    const auto validHost = [](const QString &host) { return !host.trimmed().isEmpty() && !host.contains('/') && !host.contains('\n') && !host.contains('\r'); };
    if (!next.address.contains('@') || next.username.trimmed().isEmpty() || !validHost(next.imapHost) || !validHost(next.smtpHost)
        || next.imapPort < 1 || next.imapPort > 65535 || next.smtpPort < 1 || next.smtpPort > 65535) {
        setStatus(tr("Check account settings"), tr("Enter an email address, username, valid server names and ports.")); return;
    }
    // An explicit id selects an existing account to edit; otherwise the address
    // (case-folded, whitespace trimmed) is the stable identity, and a random id is
    // the last resort so two empty drafts never collapse into one record.
    QString id = configuration.value("id").toString().trimmed();
    if (id.isEmpty()) id = next.address.trimmed().toLower();
    if (id.isEmpty()) id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    next.id = id;
    const bool editingCurrent = next.id == m_account.id;
    // A cache is bound to one account identity. For the current account, keep the
    // original restriction so single-account behaviour is unchanged; other accounts
    // are independent records and may differ freely.
    if (editingCurrent && configured() && (next.address.compare(m_account.address, Qt::CaseInsensitive) != 0
        || next.imapHost != m_account.imapHost || next.username != m_account.username)) {
        setStatus(tr("Account change requires a separate data directory"), tr("Remove and re-add the account to change its email address, username or IMAP host.")); return;
    }
    // Only the live account may reuse the session-only password held in memory.
    next.password = password.isEmpty() ? (editingCurrent ? m_account.password : QString{}) : password;
    if (!m_database->saveAccount(next)) { setStatus(tr("Could not save account"), m_database->error()); return; }
    if (!editingCurrent) {
        // Adding or editing a background account must not disturb the live mailbox.
        if (remember && !next.password.isEmpty()) m_credentials->write(credentialKey(next), next.password);
        else if (!remember) m_credentials->remove(credentialKey(next));
        emit accountsChanged();
        setStatus(tr("Account saved."));
        return;
    }
    const bool changedMailbox = m_account.address != next.address
        || m_account.username != next.username || m_account.imapHost != next.imapHost
        || m_account.imapPort != next.imapPort || m_account.imapStartTls != next.imapStartTls;
    m_restorePending = false;
    m_switchCredentialPending = false;
    m_account = next; ++m_generation; ++m_selectionGeneration;
    if (changedMailbox) {
        m_cachedOnly = true; m_offset = 0; m_hasMore = false;
        m_messages->clear(); m_folders.clear(); m_selected.clear(); m_selectedUid.clear(); m_content = {};
        m_uidValidity.clear();
        emit foldersChanged(); emit selectionChanged();
    }
    emit accountChanged(); emit accountsChanged();
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue("restoreAllowed", remember && !m_account.password.isEmpty()); settings.sync();
    if (remember && !m_account.password.isEmpty()) m_credentials->write(credentialKey(), m_account.password);
    else m_credentials->remove(credentialKey());
    startRefreshTimer();
    setStatus(tr("Account saved. Refresh to connect."));
}
void MailController::switchAccount(const QString &id) {
    const QString accountId = id.trimmed();
    if (m_busy || m_sending || m_mutating) { setStatus(tr("Finish the current operation before switching accounts")); return; }
    if (accountId.isEmpty() || accountId == m_account.id) return;
    bool exists = false;
    for (const auto &account : m_database->accounts())
        if (account.id == accountId) { exists = true; break; }
    if (!exists) return;

    ++m_generation; ++m_selectionGeneration;
    m_database->close();
    if (!m_database->open(m_root, accountId)) {
        const QString error = m_database->error();
        // Keep the previous account usable when the new binding cannot be opened.
        m_database->open(m_root, m_account.id);
        setStatus(tr("Storage unavailable"), error);
        return;
    }

    // The new account starts from an empty, cached-only view. Only this account is
    // polled; other accounts are never fetched, pushed, or searched in the background.
    m_messages->replace({});
    m_selected.clear(); m_selectedUid.clear(); m_content = {};
    m_folder = QStringLiteral("INBOX");
    m_search.clear();
    m_offset = 0; m_hasMore = false; m_uidValidity.clear(); m_cachedOnly = true;

    m_account = m_database->account();
    m_folders.clear();
    for (const auto &folder : m_database->folders())
        m_folders.append(QVariantMap{{"id", folder.id}, {"title", folder.title}, {"total", folder.total}});
    loadCached();
    // Account has no per-account interval yet; reuse the controller-wide value.
    startRefreshTimer();

    // Credentials are per account. Clear the previous session secret before an
    // asynchronous read so it can never be used for the wrong mailbox.
    m_restorePending = false;
    m_switchCredentialPending = false;
    m_account.password.clear();
    if (configured()) {
        m_restorePending = true;
        m_switchCredentialPending = true;
        m_credentials->read(credentialKey());
    }

    emit accountChanged(); emit foldersChanged(); emit selectionChanged();
    emit searchChanged(); emit folderChanged();
    setStatus(configured() ? tr("Offline · cached mail") : tr("Add your email account to get started"));
}
void MailController::removeAccount(const QString &id) {
    if (m_demo) { setStatus(tr("Preview mode"), tr("Account changes are disabled in this synthetic preview.")); return; }
    if (m_busy || m_sending || m_mutating) { setStatus(tr("Finish the current operation before switching accounts")); return; }
    const QString accountId = id.trimmed().isEmpty() ? QStringLiteral("default") : id.trimmed();
    Account target;
    bool found = false;
    for (const auto &account : m_database->accounts())
        if (account.id == accountId) { target = account; found = true; break; }
    if (!found) return;
    if (!m_database->removeAccount(accountId)) { setStatus(tr("Could not remove account"), m_database->error()); return; }
    m_credentials->remove(credentialKey(target));
    emit accountsChanged();
    if (accountId != m_account.id) return;

    const auto remaining = m_database->accounts();
    if (!remaining.isEmpty()) { switchAccount(remaining.first().id); return; }
    disconnectAccount();
    setStatus(tr("No accounts left"));
}
void MailController::refresh() {
    if (m_demo) { setStatus(tr("Preview · synthetic mail · not connected")); return; }
    if (m_busy || m_sending) return;
    if (!configured() || m_account.password.isEmpty()) { setStatus(tr("Account needs attention"), tr("Add an account and enter an application password before connecting.")); return; }
    const auto generation = ++m_generation; const auto account = m_account;
    m_busy = true; setStatus(tr("Connecting securely…"));
    QPointer<MailController> self(this);
    run([self, account, generation] {
        try {
            const auto folders = MailioBackend::folders(account);
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, folders, generation] {
                if (!self || generation != self->m_generation) return;
                self->m_database->saveFolders(folders); self->m_folders.clear();
                for (const auto &f : folders) self->m_folders.append(QVariantMap{{"id", f.id}, {"title", f.title}, {"total", f.total}});
                emit self->foldersChanged(); self->m_busy = false; self->m_offset = 0;
                self->fetchPage(false);
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const QString error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, error, generation] {
                if (!self || generation != self->m_generation) return;
                self->m_busy = false; self->m_cachedOnly = true;
                self->loadCached(); self->setStatus(self->tr("Offline · cached mail"), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::loadCached(bool append) {
    if (!m_database->isOpen()) return;
    auto page = m_database->cachedPage(m_folder, append ? m_offset : 0, pageSize, m_search, m_unreadOnly);
    if (!page.uidValidity.isEmpty()) m_uidValidity = page.uidValidity;
    if (append) m_messages->append(page.messages); else m_messages->replace(page.messages);
    m_offset = page.nextOffset; m_hasMore = page.hasMore; emit statusChanged();
}
void MailController::selectFolder(const QString &folder) {
    if (folder == m_folder) return;
    ++m_generation; ++m_selectionGeneration; m_busy = false; m_folder = folder; m_offset = 0;
    m_selected.clear(); m_selectedUid.clear(); m_content = {}; emit selectionChanged(); emit folderChanged();
    loadCached();
    if (!m_demo && !m_account.password.isEmpty() && m_search.isEmpty()) fetchPage(false);
}
void MailController::setSearch(const QString &query) {
    if (m_search == query) return;
    ++m_generation; m_busy = false; m_cachedOnly = true; m_search = query; m_offset = 0;
    emit searchChanged(); loadCached();
    const bool server = query.size() >= 2 && configured() && !m_demo
        && !m_account.password.isEmpty() && !m_uidValidity.isEmpty();
    setStatus(server ? tr("Searching server…")
                     : m_demo ? tr("Preview · search synthetic cached messages")
                              : tr("Search · cached messages only"));
    if (server) searchServer();
}
void MailController::searchServer() {
    const QString query = m_search;
    if (query.isEmpty() || !configured() || m_demo || m_account.password.isEmpty() || m_uidValidity.isEmpty()) return;
    const auto generation = m_generation; const auto account = m_account;
    const auto folder = m_folder; const auto validity = m_uidValidity;
    m_serverSearching = true; setStatus(tr("Searching server…"));
    QPointer<MailController> self(this);
    run([self, account, folder, validity, query, generation] {
        QList<QVariantMap> matches;
        bool ok = true;
        try { matches = MailioBackend::search(account, folder, validity, query, pageSize); }
        catch (...) { ok = false; }
        if (self) QMetaObject::invokeMethod(self, [self, matches, ok, generation] {
            if (!self) return;
            self->m_serverSearching = false;
            if (generation != self->m_generation) return;
            if (!ok) { self->setStatus(MailController::tr("Search · server unavailable")); return; }
            QSet<QString> seen;
            for (int row = 0; row < self->m_messages->count(); ++row) {
                const auto existing = self->m_messages->get(row);
                seen.insert(existing.value("uid").toString() + u'\n' + existing.value("uidValidity").toString());
            }
            QList<Summary> additions;
            for (const auto &match : matches) {
                const auto key = match.value("uid").toString() + u'\n' + match.value("uidValidity").toString();
                if (seen.contains(key)) continue;
                seen.insert(key); additions.append(summaryFromMap(match));
            }
            if (!additions.isEmpty()) self->m_messages->append(additions);
            self->setStatus(MailController::tr("Search · cached + server"));
        }, Qt::QueuedConnection);
    });
}
void MailController::setUnreadOnly(bool unreadOnly) {
    if (m_unreadOnly == unreadOnly) return;
    ++m_generation; m_busy = false; m_cachedOnly = true; m_unreadOnly = unreadOnly; m_offset = 0;
    emit unreadOnlyChanged(); loadCached();
    setStatus(m_demo ? tr("Preview · synthetic cached messages")
                     : m_unreadOnly ? tr("Unread · cached messages only") : tr("Cached messages only"));
}
void MailController::setNotifyEnabled(bool enabled) {
    if (m_notifyEnabled == enabled) return;
    m_notifyEnabled = enabled;
    emit notifyEnabledChanged();
}
void MailController::setRefreshInterval(int seconds) {
    seconds = qMax(60, seconds);
    if (m_refreshInterval == seconds) return;
    m_refreshInterval = seconds;
    if (m_refreshTimer.isActive()) m_refreshTimer.start(seconds * 1000);
    emit refreshIntervalChanged();
}
void MailController::startRefreshTimer() {
    if (m_demo || !configured()) { m_refreshTimer.stop(); return; }
    m_refreshTimer.start(qMax(60, m_refreshInterval) * 1000);
}
void MailController::checkNewMail(const Page &page) {
    quint64 maxUid = 0;
    for (const auto &summary : page.messages) {
        bool ok = false;
        const quint64 uid = summary.uid.toULongLong(&ok);
        if (ok && uid > maxUid) maxUid = uid;
    }
    QSettings settings;
    const QString key = QStringLiteral("accounts/%1/lastMaxUid").arg(m_account.id);
    if (!settings.contains(key)) { settings.setValue(key, QVariant::fromValue<qulonglong>(maxUid)); return; }
    const quint64 lastUid = settings.value(key).toULongLong();
    // A lower maximum means the mailbox or every UID was regenerated; re-baseline quietly.
    if (maxUid < lastUid) { settings.setValue(key, QVariant::fromValue<qulonglong>(maxUid)); return; }
    if (maxUid == lastUid) return;
    int count = 0;
    const Summary *first = nullptr;
    for (const auto &summary : page.messages) {
        bool ok = false;
        const quint64 uid = summary.uid.toULongLong(&ok);
        if (!ok || uid <= lastUid) continue;
        if (!first) first = &summary;
        ++count;
    }
    settings.setValue(key, QVariant::fromValue<qulonglong>(maxUid));
    if (!first || !m_notifyEnabled) return;
    const QString title = count == 1 ? first->sender : tr("%1 封新邮件").arg(count);
    NotificationService::notify(title, first->subject);
}
void MailController::loadMore() {
    if (m_busy || !m_hasMore) return;
    if (m_demo || m_cachedOnly || !m_search.isEmpty() || m_account.password.isEmpty()) loadCached(true);
    else fetchPage(true);
}
void MailController::fetchPage(bool append, bool quiet) {
    if (m_busy || m_demo || m_account.password.isEmpty()) return;
    if (!m_search.isEmpty()) { loadCached(append); return; }
    const auto generation = m_generation; const auto account = m_account; const auto folder = m_folder;
    const auto offset = append ? m_offset : 0;
    m_busy = true; if (!quiet) setStatus(tr("Fetching message headers…"));
    QPointer<MailController> self(this);
    run([self, account, folder, offset, generation, append, quiet] {
        try {
            auto page = MailioBackend::page(account, folder, offset, pageSize);
            if (self) QMetaObject::invokeMethod(self, [self, page, generation, append, quiet] {
                if (!self || generation != self->m_generation) return;
                self->m_busy = false;
                if (!self->m_database->savePage(page)) {
                    if (!quiet) self->setStatus(MailController::tr("Could not cache messages"), self->m_database->error());
                    return;
                }
                const bool changedValidity = !self->m_content.summary.uidValidity.isEmpty() && self->m_content.summary.folder == page.folder
                    && self->m_content.summary.uidValidity != page.uidValidity;
                const bool listChangedValidity = self->m_messages->count() > 0
                    && self->m_messages->get(0).value("uidValidity").toString() != page.uidValidity;
                if (changedValidity || listChangedValidity) {
                    ++self->m_selectionGeneration; self->m_selected.clear(); self->m_selectedUid.clear(); self->m_content = {};
                    emit self->selectionChanged();
                }
                if (append && listChangedValidity) {
                    // Offsets refer to the old mailbox generation. The returned
                    // tail cannot substitute for the new generation's first page.
                    self->m_messages->clear(); self->m_offset = 0; self->m_hasMore = false;
                    self->fetchPage(false, quiet);
                    return;
                }
                if (self->m_unreadOnly) {
                    // The network page is intentionally raw. Recompute filtered
                    // offsets against the complete cache after saving it.
                    self->loadCached(false);
                } else {
                    if (append) self->m_messages->append(page.messages); else self->m_messages->replace(page.messages);
                    self->m_offset = page.nextOffset; self->m_hasMore = page.hasMore;
                }
                self->m_cachedOnly = self->m_unreadOnly || !self->m_search.isEmpty();
                if (quiet && !append && page.folder == QLatin1String("INBOX")) self->checkNewMail(page);
                if (!quiet) self->setStatus(MailController::tr("Up to date · headers cached · server flags are read-only"));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, error, append, quiet] {
                if (!self || generation != self->m_generation) return;
                self->m_busy = false; self->m_cachedOnly = true;
                self->loadCached(append);
                if (!quiet) self->setStatus(MailController::tr("Offline · cached mail"), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::present(const Content &content) {
    m_content = content; m_selectedUid = content.summary.uid; m_selected = contentMap(content);
    emit selectionChanged();
}
void MailController::openMessage(const QString &uid, const QString &validity) {
    const auto selectionGeneration = ++m_selectionGeneration;
    if (auto cached = m_database->content(m_folder, validity, uid)) { present(*cached); return; }
    m_selectedUid = uid; m_selected.clear(); m_content = {}; emit selectionChanged();
    if (m_demo || m_account.password.isEmpty()) { setStatus(tr("This message is not cached"), tr("Connect to download its content.")); return; }
    const auto generation = m_generation; const auto account = m_account; const auto folder = m_folder;
    setStatus(tr("Loading message without changing server flags…")); QPointer<MailController> self(this);
    run([self, account, folder, validity, uid, generation, selectionGeneration] {
        try {
            auto content = MailioBackend::content(account, folder, validity, uid);
            if (self) QMetaObject::invokeMethod(self, [self, content, generation, selectionGeneration] {
                if (!self || generation != self->m_generation || selectionGeneration != self->m_selectionGeneration) return;
                if (!self->m_database->saveContent(content)) { self->setStatus(self->tr("Could not cache content"), self->m_database->error()); return; }
                self->present(content); self->setStatus(self->tr("Message cached · remote content blocked"));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, selectionGeneration, error] {
                if (self && generation == self->m_generation && selectionGeneration == self->m_selectionGeneration) self->setStatus(self->tr("Could not load message"), error);
            }, Qt::QueuedConnection);
        }
    });
}
QVariantMap MailController::newDraft(const QString &mode) {
    if (mode != "new" && mode != "reply" && mode != "replyAll" && mode != "forward") {
        setStatus(tr("Could not create draft"), tr("Choose new, reply, reply all or forward.")); return {};
    }
    if (mode != "new" && m_selected.isEmpty()) {
        setStatus(tr("Could not create draft"), tr("Select a message before replying or forwarding.")); return {};
    }
    Draft draft; draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces); draft.updated = QDateTime::currentDateTimeUtc();
    if (mode != "new") {
        const auto &s = m_content.summary;
        draft.subject = cleanSubject(mode == "forward" ? "Fwd: " : "Re: ", s.subject);
        if (mode != "forward") {
            QStringList primary;
            if (!replyAddresses(m_content.replyTo.isEmpty() ? s.senderAddress : m_content.replyTo, &primary) || primary.isEmpty()) {
                setStatus(tr("Could not create reply"), tr("This message has an unsupported recipient format. Create a new draft and enter recipients manually.")); return {};
            }
            QSet<QString> seen;
            QStringList recipients;
            for (const auto &address : primary) {
                const auto key = address.toCaseFolded();
                if (!seen.contains(key)) { seen.insert(key); recipients.append(address); }
            }
            draft.to = recipients.join(", ");
            if (mode == "replyAll") {
                const QString self = m_account.address.trimmed().toCaseFolded();
                recipients.removeIf([&self](const QString &address) { return address.toCaseFolded() == self; });
                draft.to = recipients.join(", ");
                seen.insert(self);
                QStringList others;
                for (const auto &field : {m_content.to, m_content.cc}) {
                    QStringList parsed;
                    if (!replyAddresses(field, &parsed)) {
                        setStatus(tr("Could not create reply all"), tr("This message has an unsupported recipient format. Create a new draft and enter recipients manually.")); return {};
                    }
                    for (const auto &address : parsed) {
                        const auto key = address.toCaseFolded();
                        if (!seen.contains(key)) { seen.insert(key); others.append(address); }
                    }
                }
                if (recipients.isEmpty() && !others.isEmpty()) {
                    draft.to = others.takeFirst();
                }
                draft.cc = others.join(", ");
            }
            draft.inReplyTo = m_content.messageId; draft.references = m_content.messageId;
        }
        draft.body = QStringLiteral("\n\n— %1 · %2 —\n").arg(s.sender, s.date);
        for (const auto &line : m_content.text.split('\n')) draft.body += "> " + line + '\n';
        QString quoted = m_content.html;
        if (quoted.isEmpty()) {
            quoted = escapeHtml(m_content.text);
            quoted.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        }
        draft.html = QStringLiteral("<p></p><p>— %1 · %2 —</p><blockquote>%3</blockquote>")
                         .arg(escapeHtml(s.sender), escapeHtml(s.date), quoted);
        if (mode == "forward") for (const auto &attachment : m_content.attachments) {
            const auto path = QDir(m_root).filePath("attachments/" + QUuid::createUuid().toString(QUuid::WithoutBraces));
            QDir().mkpath(QFileInfo(path).absolutePath()); QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(attachment.bytes) != attachment.bytes.size() || !file.commit()) continue;
            QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            auto copy = attachment; copy.path = path; copy.bytes.clear(); draft.attachments.append(copy);
        }
    }
    if (!m_database->saveDraft(draft)) { setStatus(tr("Could not save draft"), m_database->error()); return {}; }
    emit draftsChanged(); return draftMap(draft);
}
QVariantMap MailController::loadDraft(const QString &id) {
    for (const auto &draft : m_database->drafts()) if (draft.id == id) return draftMap(draft);
    return {};
}
QVariantMap MailController::saveDraft(const QVariantMap &map) {
    Draft draft = draftFromMap(map);
    if (draft.id.isEmpty()) draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft.updated = QDateTime::currentDateTimeUtc();
    if (!m_database->saveDraft(draft)) { setStatus(tr("Could not save draft"), m_database->error()); return {}; }
    emit draftsChanged();
    return loadDraft(draft.id);
}
void MailController::discardDraft(const QString &id) {
    if (m_sending) return;
    if (!m_database->deleteDraft(id)) setStatus(tr("Could not remove local draft"), m_database->error());
    emit draftsChanged();
}
QVariantMap MailController::addAttachment(const QString &url) {
    const QString path = localPath(url); QFile file(path); const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile() || info.size() > 15 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
        setStatus(tr("Could not attach file"), tr("Choose a readable local file smaller than 15 MiB (25 MiB total encoded message limit).")); return {};
    }
    const auto target = QDir(m_root).filePath("attachments/" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QDir().mkpath(QFileInfo(target).absolutePath()); QSaveFile out(target);
    if (!out.open(QIODevice::WriteOnly)) { setStatus(tr("Could not copy attachment"), tr("Check available disk space and permissions.")); return {}; }
    const auto bytes = file.read(attachmentLimit + 1);
    if (bytes.size() > attachmentLimit || out.write(bytes) != bytes.size() || !out.commit()) { setStatus(tr("Could not copy attachment")); return {}; }
    QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return {{"name", info.fileName()}, {"path", target}, {"mimeType", QMimeDatabase().mimeTypeForFile(info).name()}, {"size", bytes.size()}};
}
void MailController::saveAttachment(int index, const QString &url) {
    if (index < 0 || index >= m_content.attachments.size()) return;
    const auto path = localPath(url);
    if (path.isEmpty() || QFileInfo::exists(path)) { setStatus(tr("Choose a new file name"), tr("Existing files are not overwritten.")); return; }
    QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { setStatus(tr("Could not save attachment")); return; }
    const auto &bytes = m_content.attachments[index].bytes;
    if (file.write(bytes) != bytes.size()) { file.close(); file.remove(); setStatus(tr("Could not save complete attachment")); return; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner); file.close(); setStatus(tr("Attachment saved"));
}
void MailController::sendDraft(const QVariantMap &map) {
    if (m_demo) { setStatus(tr("Preview mode"), tr("Sending is disabled. Your draft is saved locally.")); saveDraft(map); return; }
    if (m_sending || m_busy) return;
    if (!configured() || m_account.password.isEmpty()) { setStatus(tr("Account needs attention"), tr("Enter an application password before sending.")); return; }
    const auto existing = loadDraft(map.value("id").toString());
    const auto existingState = existing.value("state").toString();
    if (existingState == "accepted" || existingState == "sending" || existingState == "unknown") {
        setStatus(tr("This draft may already have been sent"), tr("Create a new draft rather than automatically retransmitting it.")); return;
    }
    const auto saved = saveDraft(map); if (saved.isEmpty()) return;
    const auto draft = draftFromMap(saved);
    if (draft.state == "accepted" || draft.state == "sending" || draft.state == "unknown") { setStatus(tr("This draft may already have been sent"), tr("Create a new draft rather than automatically retransmitting it.")); return; }
    if (draft.to.trimmed().isEmpty() && draft.cc.trimmed().isEmpty() && draft.bcc.trimmed().isEmpty()) { setStatus(tr("Add a recipient")); return; }
    if (!m_database->markDraftState(draft.id, "sending")) { setStatus(tr("Could not persist send state"), m_database->error()); return; }
    emit draftsChanged(); m_sending = true; setStatus(tr("Submitting to SMTP server…"));
    const auto account = m_account; QPointer<MailController> self(this);
    run([self, account, draft] {
        SendResult result;
        QByteArray raw;
        try { result = MailioBackend::send(account, draft, &raw); }
        catch (...) { result.state = SendResult::Unknown; result.message = QStringLiteral("Connection ended before the result could be confirmed."); }
        if (self) QMetaObject::invokeMethod(self, [self, result, account, raw, id = draft.id] {
            if (!self) return;
            self->m_sending = false;
            const QString state = result.state == SendResult::Accepted ? "accepted" : result.state == SendResult::Rejected ? "rejected" : "unknown";
            self->m_database->markDraftState(id, state); emit self->draftsChanged();
            if (result.state == SendResult::Accepted) {
                self->setStatus(self->tr("SMTP server accepted this message · delivery not confirmed")); emit self->draftAccepted(id);
                self->run([self, account, raw] {
                    const auto folder = MailioBackend::sentFolder(account);
                    if (folder.isEmpty()) {
                        if (self) QMetaObject::invokeMethod(self, [self] {
                            if (self) self->setStatus(self->tr("SMTP server accepted this message · no Sent folder found, no copy saved"));
                        }, Qt::QueuedConnection);
                        return;
                    }
                    bool saved = false;
                    if (!raw.isEmpty()) {
                        try { MailioBackend::appendSent(account, folder, raw); saved = true; } catch (...) { saved = false; }
                    }
                    if (self) QMetaObject::invokeMethod(self, [self, folder, saved] {
                        if (!self) return;
                        self->setStatus(saved
                            ? self->tr("SMTP server accepted this message · copy saved to %1").arg(folder)
                            : self->tr("SMTP server accepted this message · copy could not be saved to %1").arg(folder));
                    }, Qt::QueuedConnection);
                });
            } else self->setStatus(result.state == SendResult::Unknown ? self->tr("Send result unknown · not retried") : self->tr("Message not accepted · draft preserved"), result.message);
        }, Qt::QueuedConnection);
    });
}
void MailController::clearError() { m_error.clear(); emit statusChanged(); }
void MailController::disconnectAccount() {
    if (m_sending) return;
    ++m_generation; ++m_selectionGeneration; m_pool.clear(); m_account.password.clear(); m_busy = false; m_restorePending = false; m_switchCredentialPending = false; m_cachedOnly = true;
    m_refreshTimer.stop();
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue("restoreAllowed", false); settings.sync();
    setStatus(tr("Disconnected · cached mail available"));
}
void MailController::setMessageRead(int index, bool read) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    const QVariantMap map = m_messages->get(index);
    if (map.isEmpty()) return;
    Summary summary = summaryFromMap(map);
    if (summary.unread == !read) return;
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const bool wasUnread = summary.unread;
    summary.unread = !read;
    m_messages->update(summary);
    if (!m_selected.isEmpty() && sameMessage(summaryFromMap(m_selected), summary)) {
        m_selected["unread"] = summary.unread; m_content.summary.unread = summary.unread; emit selectionChanged();
    }
    if (m_demo) { setStatus(tr("Preview · flags are local only")); return; }
    m_mutating = true; setStatus(tr("Updating message flags…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = summary.folder, validity = summary.uidValidity, uid = summary.uid;
    const QStringList flags{QStringLiteral("\\Seen")};
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uid, flags, read, wasUnread, generation] {
        try {
            MailioBackend::storeFlags(account, folder, validity, uid, flags, read);
            if (self) QMetaObject::invokeMethod(self, [self, generation, read] {
                if (!self) return;
                self->m_mutating = false;
                if (generation == self->m_generation)
                    self->setStatus(read ? MailController::tr("Message marked as read") : MailController::tr("Message marked as unread"));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, folder, validity, uid, wasUnread, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                const int row = self->m_messages->indexOf(folder, validity, uid);
                if (row >= 0) {
                    auto rollback = summaryFromMap(self->m_messages->get(row));
                    rollback.unread = wasUnread; self->m_messages->update(rollback);
                    if (!self->m_selected.isEmpty() && sameMessage(summaryFromMap(self->m_selected), rollback)) {
                        self->m_selected["unread"] = wasUnread; self->m_content.summary.unread = wasUnread;
                        emit self->selectionChanged();
                    }
                }
                self->setStatus(MailController::tr("Could not update message flags"), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::toggleFlagged(int index) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    const QVariantMap map = m_messages->get(index);
    if (map.isEmpty()) return;
    Summary summary = summaryFromMap(map);
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const bool wasFlagged = summary.flagged;
    summary.flagged = !wasFlagged;
    m_messages->update(summary);
    if (!m_selected.isEmpty() && sameMessage(summaryFromMap(m_selected), summary)) {
        m_selected["flagged"] = summary.flagged; m_content.summary.flagged = summary.flagged; emit selectionChanged();
    }
    if (m_demo) { setStatus(tr("Preview · flags are local only")); return; }
    m_mutating = true; setStatus(tr("Updating message flags…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = summary.folder, validity = summary.uidValidity, uid = summary.uid;
    const QStringList flags{QStringLiteral("\\Flagged")};
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uid, flags, add = !wasFlagged, wasFlagged, generation] {
        try {
            MailioBackend::storeFlags(account, folder, validity, uid, flags, add);
            if (self) QMetaObject::invokeMethod(self, [self, generation, add] {
                if (!self) return;
                self->m_mutating = false;
                if (generation == self->m_generation)
                    self->setStatus(add ? MailController::tr("Message flagged") : MailController::tr("Message unflagged"));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, folder, validity, uid, wasFlagged, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                const int row = self->m_messages->indexOf(folder, validity, uid);
                if (row >= 0) {
                    auto rollback = summaryFromMap(self->m_messages->get(row));
                    rollback.flagged = wasFlagged; self->m_messages->update(rollback);
                    if (!self->m_selected.isEmpty() && sameMessage(summaryFromMap(self->m_selected), rollback)) {
                        self->m_selected["flagged"] = wasFlagged; self->m_content.summary.flagged = wasFlagged;
                        emit self->selectionChanged();
                    }
                }
                self->setStatus(MailController::tr("Could not update message flags"), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::deleteMessage(int index) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    const QVariantMap map = m_messages->get(index);
    if (map.isEmpty()) return;
    const Summary summary = summaryFromMap(map);
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const auto snapshot = modelSnapshot(m_messages);
    const auto oldSelected = m_selected; const auto oldSelectedUid = m_selectedUid; const auto oldContent = m_content;
    QList<Summary> remaining;
    for (const auto &item : snapshot) if (!sameMessage(item, summary)) remaining.append(item);
    m_messages->replace(remaining);
    const bool selectedThis = !m_selectedUid.isEmpty() && m_selectedUid == summary.uid
        && (m_selected.isEmpty() || sameMessage(summaryFromMap(m_selected), summary));
    if (selectedThis) { m_selected.clear(); m_selectedUid.clear(); m_content = {}; emit selectionChanged(); }
    if (m_demo) { setStatus(tr("Preview · message removed locally")); return; }
    m_mutating = true; setStatus(tr("Deleting message…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = summary.folder, validity = summary.uidValidity, uid = summary.uid;
    const QStringList deleted{QStringLiteral("\\Deleted")};
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uid, deleted, generation, snapshot, oldSelected, oldSelectedUid, oldContent] {
        bool usedFullExpunge = false;
        try {
            MailioBackend::storeFlags(account, folder, validity, uid, deleted, true);
            MailioBackend::expunge(account, folder, validity, uid, &usedFullExpunge);
            if (self) QMetaObject::invokeMethod(self, [self, generation, folder, validity, uid, usedFullExpunge] {
                if (!self) return;
                self->m_mutating = false;
                if (self->m_database) self->m_database->removeMessage(folder, validity, uid);
                if (generation == self->m_generation) self->setStatus(usedFullExpunge
                    ? MailController::tr("Message deleted · mailbox was expunged")
                    : MailController::tr("Message deleted"));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, snapshot, oldSelected, oldSelectedUid, oldContent, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                self->m_messages->replace(snapshot);
                if (self->m_selectedUid.isEmpty()) {
                    self->m_selected = oldSelected; self->m_selectedUid = oldSelectedUid; self->m_content = oldContent;
                    emit self->selectionChanged();
                }
                self->setStatus(MailController::tr("Could not delete message"), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::archiveMessage(int index) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    QString destination;
    const QStringList names{QStringLiteral("Archive"), QStringLiteral("Archives"), QStringLiteral("All Mail")};
    for (const auto &name : names) {
        for (const auto &value : m_folders) {
            const auto folder = value.toMap();
            const auto id = folder.value("id").toString();
            if (id.isEmpty()) continue;
            if (folder.value("title").toString().compare(name, Qt::CaseInsensitive) == 0
                || id.compare(name, Qt::CaseInsensitive) == 0) { destination = id; break; }
        }
        if (!destination.isEmpty()) break;
    }
    if (destination.isEmpty()) {
        setStatus(tr("Archive folder unavailable"), tr("This account has no Archive, Archives or All Mail folder.")); return;
    }
    moveMessage(index, destination);
}
void MailController::moveMessage(int index, const QString &destination) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    if (destination.trimmed().isEmpty()) { setStatus(tr("Choose a destination folder")); return; }
    const QVariantMap map = m_messages->get(index);
    if (map.isEmpty()) return;
    const Summary summary = summaryFromMap(map);
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const auto snapshot = modelSnapshot(m_messages);
    const auto oldSelected = m_selected; const auto oldSelectedUid = m_selectedUid; const auto oldContent = m_content;
    QList<Summary> remaining;
    for (const auto &item : snapshot) if (!sameMessage(item, summary)) remaining.append(item);
    m_messages->replace(remaining);
    const bool selectedThis = !m_selectedUid.isEmpty() && m_selectedUid == summary.uid
        && (m_selected.isEmpty() || sameMessage(summaryFromMap(m_selected), summary));
    if (selectedThis) { m_selected.clear(); m_selectedUid.clear(); m_content = {}; emit selectionChanged(); }
    if (m_demo) { setStatus(tr("Preview · message moved locally")); return; }
    m_mutating = true; setStatus(tr("Moving message…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = summary.folder, validity = summary.uidValidity, uid = summary.uid;
    const bool toArchive = isArchiveFolder(m_folders, destination);
    const QStringList deleted{QStringLiteral("\\Deleted")};
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uid, destination, deleted, generation, snapshot, oldSelected, oldSelectedUid, oldContent, toArchive] {
        bool usedFullExpunge = false;
        try {
            MailioBackend::copyTo(account, folder, validity, uid, destination);
            MailioBackend::storeFlags(account, folder, validity, uid, deleted, true);
            MailioBackend::expunge(account, folder, validity, uid, &usedFullExpunge);
            if (self) QMetaObject::invokeMethod(self, [self, generation, usedFullExpunge, toArchive, folder, validity, uid] {
                if (!self) return;
                self->m_mutating = false;
                if (self->m_database) self->m_database->removeMessage(folder, validity, uid);
                if (generation != self->m_generation) return;
                if (toArchive) self->setStatus(usedFullExpunge
                    ? MailController::tr("Moved to Archive · mailbox was expunged")
                    : MailController::tr("Moved to Archive"));
                else self->setStatus(usedFullExpunge
                    ? MailController::tr("Moved · mailbox was expunged")
                    : MailController::tr("Moved"));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, snapshot, oldSelected, oldSelectedUid, oldContent, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                self->m_messages->replace(snapshot);
                if (self->m_selectedUid.isEmpty()) {
                    self->m_selected = oldSelected; self->m_selectedUid = oldSelectedUid; self->m_content = oldContent;
                    emit self->selectionChanged();
                }
                self->setStatus(MailController::tr("Could not move message"), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::setMessagesRead(const QVariantList &indices, bool read) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    QList<Summary> selected;
    for (const auto &value : indices) {
        const QVariantMap map = m_messages->get(value.toInt());
        if (map.isEmpty()) continue;
        const Summary summary = summaryFromMap(map);
        if (summary.folder != m_folder || summary.uidValidity != m_uidValidity) continue;
        selected.append(summary);
    }
    if (selected.isEmpty()) return;
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const QList<Summary> before = selected;
    const auto oldSelected = m_selected; const auto oldContent = m_content;
    bool touchedSelection = false;
    for (const auto &summary : before) {
        Summary updated = summary; updated.unread = !read;
        m_messages->update(updated);
        if (!m_selected.isEmpty() && sameMessage(summaryFromMap(m_selected), summary)) {
            m_selected["unread"] = updated.unread; m_content.summary.unread = updated.unread; touchedSelection = true;
        }
    }
    if (touchedSelection) emit selectionChanged();
    if (m_demo) { setStatus(tr("Preview · flags are local only")); return; }
    m_mutating = true; setStatus(tr("Updating message flags…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = m_folder, validity = m_uidValidity;
    QStringList uids; for (const auto &summary : before) uids << summary.uid;
    const QStringList flags{QStringLiteral("\\Seen")};
    const int count = before.size();
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uids, flags, read, count, before, oldSelected, oldContent, touchedSelection, generation] {
        try {
            MailioBackend::storeFlagsMany(account, folder, validity, uids, flags, read);
            if (self) QMetaObject::invokeMethod(self, [self, generation, read, count] {
                if (!self) return;
                self->m_mutating = false;
                if (generation == self->m_generation)
                    self->setStatus(read ? MailController::tr("%1 messages marked read").arg(count)
                                         : MailController::tr("%1 messages marked unread").arg(count));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, before, oldSelected, oldContent, touchedSelection, count, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                for (const auto &summary : before) {
                    const int row = self->m_messages->indexOf(summary.folder, summary.uidValidity, summary.uid);
                    if (row >= 0) self->m_messages->update(summary);
                }
                if (touchedSelection) {
                    self->m_selected = oldSelected; self->m_content = oldContent;
                    emit self->selectionChanged();
                }
                self->setStatus(MailController::tr("Could not update %1 messages").arg(count), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::setMessagesFlagged(const QVariantList &indices, bool flagged) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    QList<Summary> selected;
    for (const auto &value : indices) {
        const QVariantMap map = m_messages->get(value.toInt());
        if (map.isEmpty()) continue;
        const Summary summary = summaryFromMap(map);
        if (summary.folder != m_folder || summary.uidValidity != m_uidValidity) continue;
        selected.append(summary);
    }
    if (selected.isEmpty()) return;
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const QList<Summary> before = selected;
    const auto oldSelected = m_selected; const auto oldContent = m_content;
    bool touchedSelection = false;
    for (const auto &summary : before) {
        Summary updated = summary; updated.flagged = flagged;
        m_messages->update(updated);
        if (!m_selected.isEmpty() && sameMessage(summaryFromMap(m_selected), summary)) {
            m_selected["flagged"] = updated.flagged; m_content.summary.flagged = updated.flagged; touchedSelection = true;
        }
    }
    if (touchedSelection) emit selectionChanged();
    if (m_demo) { setStatus(tr("Preview · flags are local only")); return; }
    m_mutating = true; setStatus(tr("Updating message flags…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = m_folder, validity = m_uidValidity;
    QStringList uids; for (const auto &summary : before) uids << summary.uid;
    const QStringList flags{QStringLiteral("\\Flagged")};
    const int count = before.size();
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uids, flags, flagged, count, before, oldSelected, oldContent, touchedSelection, generation] {
        try {
            MailioBackend::storeFlagsMany(account, folder, validity, uids, flags, flagged);
            if (self) QMetaObject::invokeMethod(self, [self, generation, flagged, count] {
                if (!self) return;
                self->m_mutating = false;
                if (generation == self->m_generation)
                    self->setStatus(flagged ? MailController::tr("%1 messages flagged").arg(count)
                                            : MailController::tr("%1 messages unflagged").arg(count));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, before, oldSelected, oldContent, touchedSelection, count, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                for (const auto &summary : before) {
                    const int row = self->m_messages->indexOf(summary.folder, summary.uidValidity, summary.uid);
                    if (row >= 0) self->m_messages->update(summary);
                }
                if (touchedSelection) {
                    self->m_selected = oldSelected; self->m_content = oldContent;
                    emit self->selectionChanged();
                }
                self->setStatus(MailController::tr("Could not update %1 messages").arg(count), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::deleteMessages(const QVariantList &indices) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    QList<Summary> selected;
    for (const auto &value : indices) {
        const QVariantMap map = m_messages->get(value.toInt());
        if (map.isEmpty()) continue;
        const Summary summary = summaryFromMap(map);
        if (summary.folder != m_folder || summary.uidValidity != m_uidValidity) continue;
        selected.append(summary);
    }
    if (selected.isEmpty()) return;
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const auto snapshot = modelSnapshot(m_messages);
    const auto oldSelected = m_selected; const auto oldSelectedUid = m_selectedUid; const auto oldContent = m_content;
    QList<Summary> remaining;
    for (const auto &item : snapshot) {
        bool remove = false;
        for (const auto &summary : selected) if (sameMessage(item, summary)) { remove = true; break; }
        if (!remove) remaining.append(item);
    }
    m_messages->replace(remaining);
    bool selectedRemoved = false;
    for (const auto &summary : selected) {
        if (!m_selectedUid.isEmpty() && m_selectedUid == summary.uid
            && (m_selected.isEmpty() || sameMessage(summaryFromMap(m_selected), summary))) { selectedRemoved = true; break; }
    }
    if (selectedRemoved) { m_selected.clear(); m_selectedUid.clear(); m_content = {}; emit selectionChanged(); }
    if (m_demo) { setStatus(tr("Preview · messages removed locally")); return; }
    m_mutating = true; setStatus(tr("Deleting messages…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = m_folder, validity = m_uidValidity;
    QStringList uids; for (const auto &summary : selected) uids << summary.uid;
    const int count = selected.size();
    const QStringList deleted{QStringLiteral("\\Deleted")};
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uids, deleted, count, generation, snapshot, oldSelected, oldSelectedUid, oldContent] {
        try {
            MailioBackend::storeFlagsMany(account, folder, validity, uids, deleted, true);
            MailioBackend::expungeMany(account, folder, validity, uids);
            if (self) QMetaObject::invokeMethod(self, [self, generation, folder, validity, uids, count] {
                if (!self) return;
                self->m_mutating = false;
                if (self->m_database) for (const auto &uid : uids) self->m_database->removeMessage(folder, validity, uid);
                if (generation == self->m_generation)
                    self->setStatus(MailController::tr("%1 messages deleted").arg(count));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, snapshot, oldSelected, oldSelectedUid, oldContent, count, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                self->m_messages->replace(snapshot);
                if (self->m_selectedUid.isEmpty()) {
                    self->m_selected = oldSelected; self->m_selectedUid = oldSelectedUid; self->m_content = oldContent;
                    emit self->selectionChanged();
                }
                self->setStatus(MailController::tr("Could not update %1 messages").arg(count), error);
            }, Qt::QueuedConnection);
        }
    });
}
void MailController::archiveMessages(const QVariantList &indices) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    QString destination;
    const QStringList names{QStringLiteral("Archive"), QStringLiteral("Archives"), QStringLiteral("All Mail")};
    for (const auto &name : names) {
        for (const auto &value : m_folders) {
            const auto folder = value.toMap();
            const auto id = folder.value("id").toString();
            if (id.isEmpty()) continue;
            if (folder.value("title").toString().compare(name, Qt::CaseInsensitive) == 0
                || id.compare(name, Qt::CaseInsensitive) == 0) { destination = id; break; }
        }
        if (!destination.isEmpty()) break;
    }
    if (destination.isEmpty()) {
        setStatus(tr("Archive folder unavailable"), tr("This account has no Archive, Archives or All Mail folder.")); return;
    }
    moveMessagesTo(indices, destination, true);
}
void MailController::moveMessages(const QVariantList &indices, const QString &destination) {
    if (m_mutating) { setStatus(tr("Another mail operation is in progress")); return; }
    if (destination.trimmed().isEmpty()) { setStatus(tr("Choose a destination folder")); return; }
    moveMessagesTo(indices, destination, false);
}
void MailController::moveMessagesTo(const QVariantList &indices, const QString &destination, bool archive) {
    QList<Summary> selected;
    for (const auto &value : indices) {
        const QVariantMap map = m_messages->get(value.toInt());
        if (map.isEmpty()) continue;
        const Summary summary = summaryFromMap(map);
        if (summary.folder != m_folder || summary.uidValidity != m_uidValidity) continue;
        selected.append(summary);
    }
    if (selected.isEmpty()) return;
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages.")); return;
    }
    const auto snapshot = modelSnapshot(m_messages);
    const auto oldSelected = m_selected; const auto oldSelectedUid = m_selectedUid; const auto oldContent = m_content;
    QList<Summary> remaining;
    for (const auto &item : snapshot) {
        bool remove = false;
        for (const auto &summary : selected) if (sameMessage(item, summary)) { remove = true; break; }
        if (!remove) remaining.append(item);
    }
    m_messages->replace(remaining);
    bool selectedRemoved = false;
    for (const auto &summary : selected) {
        if (!m_selectedUid.isEmpty() && m_selectedUid == summary.uid
            && (m_selected.isEmpty() || sameMessage(summaryFromMap(m_selected), summary))) { selectedRemoved = true; break; }
    }
    if (selectedRemoved) { m_selected.clear(); m_selectedUid.clear(); m_content = {}; emit selectionChanged(); }
    if (m_demo) { setStatus(archive ? tr("Preview · messages archived locally") : tr("Preview · messages moved locally")); return; }
    m_mutating = true; setStatus(archive ? tr("Archiving messages…") : tr("Moving messages…"));
    const auto account = m_account; const auto generation = m_generation;
    const auto folder = m_folder, validity = m_uidValidity;
    QStringList uids; for (const auto &summary : selected) uids << summary.uid;
    const int count = selected.size();
    const QStringList deleted{QStringLiteral("\\Deleted")};
    QPointer<MailController> self(this);
    run([self, account, folder, validity, uids, destination, deleted, count, archive, generation, snapshot, oldSelected, oldSelectedUid, oldContent] {
        try {
            MailioBackend::copyMany(account, folder, validity, uids, destination);
            MailioBackend::storeFlagsMany(account, folder, validity, uids, deleted, true);
            MailioBackend::expungeMany(account, folder, validity, uids);
            if (self) QMetaObject::invokeMethod(self, [self, generation, folder, validity, uids, count, archive] {
                if (!self) return;
                self->m_mutating = false;
                if (self->m_database) for (const auto &uid : uids) self->m_database->removeMessage(folder, validity, uid);
                if (generation != self->m_generation) return;
                self->setStatus(archive ? MailController::tr("%1 messages archived").arg(count)
                                        : MailController::tr("%1 messages moved").arg(count));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, snapshot, oldSelected, oldSelectedUid, oldContent, count, error] {
                if (!self) return;
                self->m_mutating = false;
                if (generation != self->m_generation) return;
                self->m_messages->replace(snapshot);
                if (self->m_selectedUid.isEmpty()) {
                    self->m_selected = oldSelected; self->m_selectedUid = oldSelectedUid; self->m_content = oldContent;
                    emit self->selectionChanged();
                }
                self->setStatus(MailController::tr("Could not update %1 messages").arg(count), error);
            }, Qt::QueuedConnection);
        }
    });
}
}
