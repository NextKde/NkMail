#include "MailController.h"
#include "services/AccountSetup.h"
#include "MessageListModel.h"
#include "NotificationService.h"
#include "storage/MailDatabase.h"
#include "services/AccountSession.h"
#include "services/DraftService.h"
#include "services/SendService.h"
#include "services/MailboxSync.h"
#include "services/RemoteImageLoader.h"
#include "mail/MailTransport.h"
#include <QDir>
#include <QPointer>
#include <QSettings>
#include <QSet>
#include <QTimer>

namespace nkmail {
namespace {
constexpr int pageSize = 50;
bool sameMessage(const Summary &a, const Summary &b) {
    return a.folder == b.folder && a.uidValidity == b.uidValidity && a.uid == b.uid;
}
bool isArchiveFolder(const QVariantList &folders, const QString &destination) {
    const QStringList names{QStringLiteral("Archive"), QStringLiteral("Archives"), QStringLiteral("All Mail")};
    for (const auto &value : folders) {
        const auto folder = value.toMap();
        const auto id = folder.value("id").toString();
        if (id != destination) continue;
        const auto role = folder.value("role").toString();
        if (role == "archive" || role == "all") return true;
        const auto title = folder.value("title").toString();
        for (const auto &name : names)
            if (title.compare(name, Qt::CaseInsensitive) == 0 || id.compare(name, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}
}
MailController::MailController(bool demo, const QString &dataRoot, QObject *parent, std::shared_ptr<MailTransport> transport)
    : QObject(parent), m_transport(transport ? std::move(transport) : std::make_shared<MailTransport>()), m_demo(demo), m_root(dataRoot), m_database(std::make_unique<MailDatabase>()),
      m_messages(new MessageListModel(this)), m_credentials(new AccountSession(dataRoot, this)) {
    m_accountSetup = new AccountSetup(m_transport, !demo, this);
    m_drafts = new DraftService(*m_database, dataRoot, this);
    connect(m_drafts, &DraftService::draftsChanged, this, &MailController::draftsChanged);
    connect(m_drafts, &DraftService::status, this, &MailController::setStatus);
    m_send = new SendService(demo, *m_database, *m_drafts, m_transport, this);
    connect(m_send, &SendService::draftsChanged, this, &MailController::draftsChanged);
    connect(m_send, &SendService::sendPendingChanged, this, &MailController::sendPendingChanged);
    connect(m_send, &SendService::draftAccepted, this, &MailController::draftAccepted);
    connect(m_send, &SendService::status, this, &MailController::setStatus);
    m_images = new RemoteImageLoader(this);
    m_images->configurePermissions(QDir(m_root).filePath("image-permissions.ini"));
    connect(m_images, &RemoteImageLoader::loaded, this, [this](const QString &key, const QString &html) {
        if (key != RemoteImageLoader::key(m_account.id, m_content.summary, m_content.bodyFormatVersion)) return;
        if (!m_content.displayHtml.isEmpty()) m_content.displayHtml = html;
        else m_content.html = html;
        m_selected = contentMap(m_content); emit selectionChanged();
    });
    connect(m_images, &RemoteImageLoader::loadingChanged, this, [this](const QString &key) {
        if (key == RemoteImageLoader::key(m_account.id, m_content.summary, m_content.bodyFormatVersion)) emit selectionChanged();
    });
    m_commands = new MailboxCommands(*m_database, m_transport, this);
    connect(m_commands, &MailboxCommands::completed, this, [this](const QList<Summary> &messages, bool removed, const QString &error) {
        for (const auto &s : messages) {
            if (removed) {
                const int row = m_messages->indexOf(s.folder, s.uidValidity, s.uid);
                if (row >= 0) m_messages->remove(row);
            } else m_messages->update(s);
            if (sameMessage(m_content.summary, s)) {
                if (removed) { ++m_selectionGeneration; m_content = {}; m_selected.clear(); m_selectedUid.clear(); }
                else {
                    m_content.summary = s;
                    // Flag updates must preserve a pending body/error state.
                    m_selected.insert("unread", s.unread);
                    m_selected.insert("flagged", s.flagged);
                    m_selected.insert("summary", summaryMap(s));
                }
                emit selectionChanged();
            }
        }
        m_accountUnread.remove(m_account.id); ++m_accountUnreadRevision;
        emit accountUnreadChanged(); emit folderUnreadChanged();
        if (!m_smartMailbox.isEmpty()) loadCached();
        setStatus(error.isEmpty() ? tr("Messages updated") : tr("Mailbox operation needs attention"), error);
    });
    m_sync = new MailboxSync(m_transport, this);
    m_refreshTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] {
        if (m_busy || m_commands->busy() || sending() || m_demo || !configured()) return;
        fetchPage(false, true);

        for (const auto &account : m_database->accounts()) {
            if (account.id == m_account.id) continue;
            const auto generation = m_generation;
            QPointer<MailController> self(this);
            m_credentials->resolve(account, [self, generation](const Account &authenticated) {
                if (!self || generation != self->m_generation || authenticated.password.isEmpty()) return;
                self->m_sync->stats(authenticated, [self, authenticated, generation](const InboxStats &stats, const QString &error) {
                    if (!self || generation != self->m_generation || !error.isEmpty()) return;
                    self->m_accountUnread.insert(authenticated.id, stats.unseen);
                    ++self->m_accountUnreadRevision;
                    emit self->accountUnreadChanged();
                    QSettings settings(QDir(self->m_root).filePath("polling.ini"), QSettings::IniFormat);
                    const QString key = self->credentialKey(authenticated);
                    const QString validity = settings.value(key + "/validity").toString();
                    const quint64 previous = settings.value(key + "/maxUid").toULongLong();
                    const quint64 current = stats.maxUid.toULongLong();
                    settings.setValue(key + "/validity", stats.uidValidity);
                    settings.setValue(key + "/maxUid", current);
                    if (self->m_notifyEnabled && validity == stats.uidValidity && current > previous)
                        NotificationService::notify(authenticated.name, tr("New mail available"));
                });
            });
        }
    });
    connect(m_credentials, &AccountSession::passwordReady, this, [this](const QString &id, const QString &password) {
        if (id != credentialKey() || m_demo) return;
        if (m_account.authMethod == "google_oauth" && !m_restorePending && !m_account.password.isEmpty()) {
            m_account.password = password; return; // Rotated refresh token, already persisted by AccountSession.
        }
        if (!m_restorePending || !m_account.password.isEmpty()) return;
        m_restorePending = false;
        m_switchCredentialPending = false;
        m_account.password = password;
        setStatus(tr("Saved credentials restored. Refresh to connect."));
        if (m_smartMailbox == QLatin1String("allInbox") && m_selected.value("bodyPending").toBool()) {
            const auto selected = m_content.summary;
            openMessage(selected.uid, selected.uidValidity, selected.folder, m_account.id);
        }
    });
    connect(m_credentials, &AccountSession::passwordSaved, this, [this](const QString &) {
        QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
        if (!settings.value("restoreAllowed", false).toBool()) return;
        if (!m_busy && !sending()) setStatus(tr("Credentials saved in the system keychain."));
    });
    connect(m_credentials, &AccountSession::error, this, [this](const QString &id, const QString &) {
        if (id != credentialKey()) return;
        m_restorePending = false;
        if (m_switchCredentialPending) {
            // A switch could not restore a stored secret (most often no entry).
            // Stay on the newly selected account and ask for its password.
            m_switchCredentialPending = false;
            m_account.password.clear();
            if (m_account.authMethod == "google_oauth") setStatus(tr("Account needs attention"), tr("Enter a Google app password in account settings to reconnect this account."));
            else if (m_account.imapHost == "imap.qq.com") setStatus(tr("Account needs attention"), tr("Reconnect with your QQ Mail authorization code. Enable Remember password to restore it after restarting."));
            else setStatus(tr("Account needs a password"), tr("Enter an application password for this account."));
            return;
        }
        setStatus(tr("System keychain needs attention"), tr("Credential storage or removal could not be completed. The local restore permission is authoritative; no plaintext fallback is used."));
    });
    QTimer::singleShot(0, this, &MailController::initialize);
}
MailController::~MailController() { delete m_sync; delete m_commands; delete m_send; delete m_images; }
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
QStringList MailController::favoriteEntries() const {
    QSettings settings(QDir(m_root).filePath("favorites.ini"), QSettings::IniFormat);
    return settings.value("list").toStringList();
}
void MailController::saveFavoriteEntries(const QStringList &entries) const {
    QSettings settings(QDir(m_root).filePath("favorites.ini"), QSettings::IniFormat);
    settings.setValue("list", entries);
    settings.sync();
}
QString MailController::favoriteAccountName(const QString &accountId) const {
    const auto label = [](const Account &account) {
        return account.name.isEmpty() ? account.address : account.name;
    };
    if (accountId == m_account.id) return label(m_account);
    if (m_database && m_database->isOpen())
        for (const auto &account : m_database->accounts())
            if (account.id == accountId) return label(account);
    return {};
}
QVariantList MailController::favorites() const {
    QVariantList out;
    for (const auto &entry : favoriteEntries()) {
        // Each record is tab separated: accountId, folderId, title, accountName.
        // Missing trailing columns are tolerated so older or hand-edited entries
        // still resolve to a usable folder.
        const auto parts = entry.split(QLatin1Char('\t'));
        if (parts.size() < 2 || parts.at(0).isEmpty() || parts.at(1).isEmpty()) continue;
        bool accountExists = false;
        for (const auto &account : m_database->accounts())
            if (account.id == parts.at(0)) { accountExists = true; break; }
        if (!accountExists) continue;
        QString accountName = parts.size() > 3 ? parts.at(3) : QString{};
        if (accountName.isEmpty()) accountName = favoriteAccountName(parts.at(0));
        out.append(QVariantMap{{"accountId", parts.at(0)},
                               {"folderId", parts.at(1)},
                               {"title", parts.size() > 2 && !parts.at(2).isEmpty() ? parts.at(2) : parts.at(1)},
                               {"accountName", accountName}});
    }
    return out;
}
bool MailController::isFavorite(const QString &accountId, const QString &folderId) const {
    for (const auto &entry : favoriteEntries()) {
        const auto parts = entry.split(QLatin1Char('\t'));
        if (parts.size() >= 2 && parts.at(0) == accountId && parts.at(1) == folderId) return true;
    }
    return false;
}
void MailController::toggleFavorite(const QString &accountId, const QString &folderId, const QString &title) {
    if (accountId.isEmpty() || folderId.isEmpty()) return;
    QStringList entries = favoriteEntries();
    int existing = -1;
    for (int i = 0; i < entries.size(); ++i) {
        const auto parts = entries.at(i).split(QLatin1Char('\t'));
        if (parts.size() >= 2 && parts.at(0) == accountId && parts.at(1) == folderId) { existing = i; break; }
    }
    if (existing >= 0) entries.removeAt(existing);
    else entries.append(accountId + QLatin1Char('\t') + folderId + QLatin1Char('\t') + title
                        + QLatin1Char('\t') + favoriteAccountName(accountId));
    saveFavoriteEntries(entries);
    emit favoritesChanged();
}
void MailController::openFavorite(const QString &accountId, const QString &folderId) {
    if (accountId.isEmpty() || folderId.isEmpty()) return;
    if (accountId != m_account.id) {
        switchAccount(accountId);
        // switchAccount() is refused while another operation is in flight or when
        // the account no longer exists; stay put rather than selecting a folder in
        // the wrong account.
        if (m_account.id != accountId) return;
    }
    selectFolder(folderId);
}
void MailController::setStatus(const QString &status, const QString &error) {
    m_status = status; m_error = error.isEmpty() ? QString()
        : QCoreApplication::translate("MailErrors", error.toUtf8().constData()); emit statusChanged();
}
QString MailController::credentialKey() const { return credentialKey(m_account); }
QString MailController::credentialKey(const Account &account) const { return m_credentials->key(account); }
void MailController::initialize() {
    if (!m_database->open(m_root, m_account.id)) { setStatus(tr("Storage unavailable"), m_database->error()); return; }
    if (!m_database->recoverInterruptedSends()) { setStatus(tr("Storage unavailable"), m_database->error()); return; }
    if (m_demo) { seedDemo(); return; }
    m_account = m_database->account();
    if (!configured() && !m_database->accounts().isEmpty()) {
        const auto first = m_database->accounts().first();
        if (!m_database->open(m_root, first.id)) { setStatus(tr("Storage unavailable"), m_database->error()); return; }
        m_account = m_database->account();
    }
    for (const auto &folder : m_database->folders()) m_folders.append(QVariantMap{{"id", folder.id}, {"title", folder.title}, {"total", folder.total}, {"role", folder.role}});
    emit accountChanged(); emit foldersChanged(); emit draftsChanged();
    loadCached();
    setStatus(configured() ? tr("Offline · cached mail") : tr("Add your email account to get started"));
    startRefreshTimer();
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    if (configured() && settings.value(credentialKey() + "/restoreAllowed", settings.value("restoreAllowed", false)).toBool()) {
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
    for (const auto &f : folders) m_folders.append(QVariantMap{{"id", f.id}, {"title", f.title}, {"total", f.total}, {"role", f.role}});
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
bool MailController::configureGoogleAccount(const QString &existingId) {
    if (m_demo || m_busy || sending() || m_commands->busy()) return false;
    auto *setup = static_cast<AccountSetup *>(m_accountSetup);
    auto next = setup->authorizedGoogleAccount();
    if (next.authMethod != "google_oauth" || next.password.isEmpty()) return false;
    if (!existingId.isEmpty()) for (const auto &known : m_database->accounts())
        if (known.id == existingId && known.address.compare(next.address, Qt::CaseInsensitive) == 0 && !known.name.isEmpty()) next.name = known.name;
    auto configuration = accountMap(next);
    configuration.insert("id", existingId.isEmpty() ? next.address.toLower() : existingId);
    const bool saved = configureAccountImpl(configuration, next.password, true);
    if (saved) {
        setup->takeGoogleAccount();
        if (m_account.id == configuration.value("id").toString()) QTimer::singleShot(0, this, &MailController::refresh);
    }
    return saved;
}
bool MailController::configureAccount(const QVariantMap &configuration, const QString &password, bool remember) {
    auto ordinary = configuration; ordinary.insert("authMethod", "password");
    return configureAccountImpl(ordinary, password, remember);
}
bool MailController::configureAccountImpl(const QVariantMap &configuration, const QString &password, bool remember) {
    if (m_demo) { setStatus(tr("Preview mode"), tr("Account changes are disabled in this synthetic preview.")); return false; }
    if (m_busy || sending() || m_commands->busy()) return false;
    Account next = accountFromMap(configuration);
    const auto validHost = [](const QString &host) { return !host.trimmed().isEmpty() && !host.contains('/') && !host.contains('\n') && !host.contains('\r'); };
    if (!next.address.contains('@') || next.username.trimmed().isEmpty() || !validHost(next.imapHost) || !validHost(next.smtpHost)
        || next.imapPort < 1 || next.imapPort > 65535 || next.smtpPort < 1 || next.smtpPort > 65535) {
        setStatus(tr("Check account settings"), tr("Enter an email address, username, valid server names and ports.")); return false;
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
        setStatus(tr("Account change requires a separate data directory"), tr("Remove and re-add the account to change its email address, username or IMAP host.")); return false;
    }
    // Only the live account may reuse the session-only password held in memory.
    next.password = password.isEmpty() ? (editingCurrent && next.authMethod == m_account.authMethod ? m_account.password : QString{}) : password;
    if (!m_database->saveAccount(next)) { setStatus(tr("Could not save account"), m_database->error()); return false; }
    m_sync->resetReadConnection();
    if (editingCurrent && next.authMethod != m_account.authMethod) m_credentials->forget(m_account);
    m_credentials->setPassword(next);
    if (!editingCurrent) {
        // Adding or editing a background account must not disturb the live mailbox.
        if (remember && !next.password.isEmpty()) m_credentials->write(credentialKey(next), next.password);
        else if (!remember) m_credentials->remove(credentialKey(next));
        emit accountsChanged();
        if (!configured()) switchAccount(next.id);
        setStatus(tr("Account saved."));
        return true;
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
    return true;
}
void MailController::switchAccount(const QString &id, bool keepUnifiedInbox) {
    keepUnifiedInbox = keepUnifiedInbox && m_smartMailbox == QLatin1String("allInbox");
    const QString accountId = id.trimmed();
    if (m_busy || sending() || m_commands->busy()) { setStatus(tr("Finish the current operation before switching accounts")); return; }
    if (accountId.isEmpty() || accountId == m_account.id) return;
    bool exists = false;
    for (const auto &account : m_database->accounts())
        if (account.id == accountId) { exists = true; break; }
    if (!exists) return;
    m_sync->resetReadConnection();

    clearPendingSend();
    ++m_generation; ++m_selectionGeneration;
    m_database->close();
    if (!m_database->open(m_root, accountId)) {
        const QString error = m_database->error();
        // Keep the previous account usable when the new binding cannot be opened.
        m_database->open(m_root, m_account.id);
        setStatus(tr("Storage unavailable"), error);
        return;
    }

    // Foreground view starts cached-only; background accounts poll inbox statistics.
    if (!keepUnifiedInbox) m_messages->replace({});
    m_selected.clear(); m_selectedUid.clear(); m_content = {};
    m_folder = QStringLiteral("INBOX");
    if (!keepUnifiedInbox) { m_smartMailbox.clear(); m_search.clear(); }
    if (!keepUnifiedInbox) { m_offset = 0; m_hasMore = false; }
    m_uidValidity.clear(); m_cachedOnly = true;

    m_account = m_database->account();
    m_folders.clear();
    for (const auto &folder : m_database->folders())
        m_folders.append(QVariantMap{{"id", folder.id}, {"title", folder.title}, {"total", folder.total}, {"role", folder.role}});
    if (!keepUnifiedInbox) loadCached();
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
    emit searchChanged(); emit folderChanged(); emit draftsChanged();
    setStatus(configured() ? tr("Offline · cached mail") : tr("Add your email account to get started"));
}
QVariantList MailController::foldersFor(const QString &accountId) const {
    QVariantList out;
    if (!m_database->isOpen()) return out;
    for (const auto &folder : m_database->folders(accountId))
        out.append(QVariantMap{{"id", folder.id}, {"title", folder.title}, {"total", folder.total}, {"role", folder.role}});
    return out;
}
QVariantMap MailController::folderUnread() const {
    QVariantMap out;
    if (!m_database || !m_database->isOpen()) return out;
    const auto counts = m_database->unreadCounts();
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        out.insert(it.key(), it.value());
    return out;
}
QVariantMap MailController::folderUnreadFor(const QString &accountId) const {
    QVariantMap out;
    if (!m_database || !m_database->isOpen()) return out;
    const auto counts = m_database->unreadCounts(accountId);
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) out.insert(it.key(), it.value());
    return out;
}
int MailController::accountUnread(const QString &accountId) const {
    // Prefer the count most recently reported by the server; fall back to the
    // local cache when this account has not been polled yet.
    if (m_accountUnread.contains(accountId)) return m_accountUnread.value(accountId);
    // A scoped SELECT, without opening a connection or changing the active binding.
    return m_database->unreadCounts(accountId).value(QStringLiteral("INBOX"), 0);
}
void MailController::removeAccount(const QString &id) {
    if (m_demo) { setStatus(tr("Preview mode"), tr("Account changes are disabled in this synthetic preview.")); return; }
    if (m_busy || sending() || m_commands->busy()) { setStatus(tr("Finish the current operation before switching accounts")); return; }
    const QString accountId = id.trimmed().isEmpty() ? QStringLiteral("default") : id.trimmed();
    Account target;
    bool found = false;
    for (const auto &account : m_database->accounts())
        if (account.id == accountId) { target = account; found = true; break; }
    if (!found) return;
    if (!m_database->removeAccount(accountId)) { setStatus(tr("Could not remove account"), m_database->error()); return; }
    QStringList retainedFavorites;
    for (const auto &entry : favoriteEntries())
        if (entry.section(QLatin1Char('\t'), 0, 0) != accountId) retainedFavorites.append(entry);
    saveFavoriteEntries(retainedFavorites);
    emit favoritesChanged();
    m_sync->resetReadConnection();
    m_credentials->forget(target);
    m_images->forgetAccount(accountId);
    emit accountsChanged();
    if (accountId != m_account.id) {
        if (m_smartMailbox == QLatin1String("allInbox")) loadCached();
        return;
    }

    const auto remaining = m_database->accounts();
    if (!remaining.isEmpty()) { switchAccount(remaining.first().id); return; }
    disconnectAccount();
    if (!m_database->open(m_root)) { setStatus(tr("Storage unavailable"), m_database->error()); return; }
    m_account = {}; m_folders.clear(); m_messages->clear();
    m_selected.clear(); m_selectedUid.clear(); m_content = {}; m_uidValidity.clear();
    emit accountChanged(); emit foldersChanged(); emit selectionChanged(); emit draftsChanged();
    setStatus(tr("No accounts left"));
}
void MailController::refresh() {
    if (!m_smartMailbox.isEmpty()) { loadCached(); setStatus(m_smartMailbox == QLatin1String("allInbox") ? tr("All inboxes · cached messages · read-only") : tr("Smart mailbox · current account cached messages")); return; }
    if (m_demo) { setStatus(tr("Preview · synthetic mail · not connected")); return; }
    if (m_busy || sending() || m_commands->busy()) return;
    if (!configured() || m_account.password.isEmpty()) {
        if (m_account.imapHost == "imap.qq.com") setStatus(tr("Account needs attention"), tr("Reconnect with your QQ Mail authorization code. Enable Remember password to restore it after restarting."));
        else if (m_account.authMethod == "google_oauth") setStatus(tr("Account needs attention"), tr("Enter a Google app password in account settings to reconnect this account."));
        else setStatus(tr("Account needs attention"), tr("Add an account and enter an application password before connecting."));
        return;
    }
    const auto generation = ++m_generation; const auto account = m_account;
    m_busy = true; setStatus(tr("Connecting securely…"));
    m_sync->folders(account, [this, generation](const QList<Folder> &folders, const QString &error) {
        if (generation != m_generation) return;
        m_busy = false;
        if (!error.isEmpty()) { m_cachedOnly = true; loadCached(); setStatus(tr("Offline · cached mail"), error); return; }
        m_database->saveFolders(folders); m_folders.clear();
        for (const auto &f : folders) m_folders.append(QVariantMap{{"id", f.id}, {"title", f.title}, {"total", f.total}, {"role", f.role}});
        emit foldersChanged(); m_busy = false; m_offset = 0;
        fetchPage(false);
    });
}

void MailController::loadCached(bool append) {
    if (!m_database->isOpen()) return;
    auto page = m_smartMailbox.isEmpty()
        ? m_database->cachedPage(m_folder, append ? m_offset : 0, pageSize, m_search, m_unreadOnly)
        : m_database->smartPage(m_smartMailbox, append ? m_offset : 0, pageSize, m_search, m_unreadOnly);
    m_uidValidity = page.uidValidity;
    if (append) m_messages->append(page.messages); else m_messages->replace(page.messages);
    m_offset = page.nextOffset; m_hasMore = page.hasMore; emit statusChanged();
    emit folderUnreadChanged();
}
void MailController::selectSmartMailbox(const QString &kind) {
    if (kind != "flagged" && kind != "unread" && kind != "attachments" && kind != "allInbox") return;
    if (m_commands->busy()) return;
    ++m_generation; ++m_selectionGeneration; m_busy = false; m_offset = 0;
    m_smartMailbox = kind; m_cachedOnly = true; m_search.clear(); m_unreadOnly = false;
    m_selected.clear(); m_selectedUid.clear(); m_content = {};
    emit folderChanged(); emit selectionChanged(); emit searchChanged(); emit unreadOnlyChanged();
    loadCached();
    setStatus(m_smartMailbox == QLatin1String("allInbox") ? tr("All inboxes · cached messages · read-only") : tr("Smart mailbox · current account cached messages"));
}
void MailController::selectFolder(const QString &folder) {
    if (folder == m_folder && m_smartMailbox.isEmpty()) return;
    m_smartMailbox.clear();
    ++m_generation; ++m_selectionGeneration; m_busy = false; m_folder = folder; m_offset = 0;
    m_selected.clear(); m_selectedUid.clear(); m_content = {}; emit selectionChanged(); emit folderChanged();
    loadCached();
    if (!m_demo && !m_account.password.isEmpty() && m_search.isEmpty()) fetchPage(false);
}
void MailController::setSearch(const QString &query) {
    if (m_search == query) return;
    ++m_generation; m_busy = false; m_cachedOnly = true; m_search = query; m_offset = 0;
    emit searchChanged(); loadCached();
    const bool server = m_smartMailbox.isEmpty() && query.size() >= 2 && configured() && !m_demo
        && !m_account.password.isEmpty() && !m_uidValidity.isEmpty();
    setStatus(server ? tr("Searching server…")
                     : m_demo ? tr("Preview · search synthetic cached messages")
                              : tr("Search · cached messages only"));
    if (server) searchServer();
}
void MailController::searchServer() {
    const QString query = m_search;
    if (!m_smartMailbox.isEmpty() || query.isEmpty() || !configured() || m_demo || m_account.password.isEmpty() || m_uidValidity.isEmpty()) return;
    const auto generation = m_generation; const auto account = m_account;
    const auto folder = m_folder; const auto validity = m_uidValidity;
    m_serverSearching = true; setStatus(tr("Searching server…"));
    m_sync->search(account, folder, validity, query, pageSize, [this, generation](const QList<QVariantMap> &matches, const QString &error) {
        if (generation != m_generation) return;
        m_serverSearching = false;
        if (!error.isEmpty()) { setStatus(tr("Search · server unavailable"), error); return; }
        QSet<QString> seen;
        for (int row = 0; row < m_messages->count(); ++row) {
            const auto existing = m_messages->get(row);
            seen.insert(existing.value("uid").toString() + u'\n' + existing.value("uidValidity").toString());
        }
        QList<Summary> additions;
        for (const auto &match : matches) {
            const auto key = match.value("uid").toString() + u'\n' + match.value("uidValidity").toString();
            if (seen.contains(key)) continue;
            seen.insert(key); additions.append(summaryFromMap(match));
        }
        if (!additions.isEmpty()) m_messages->append(additions);
        setStatus(MailController::tr("Search · cached + server"));
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
void MailController::setSendDelay(int seconds) {
    if (sendDelay() == qBound(0, seconds, 60)) return;
    m_send->setDelay(seconds); emit sendDelayChanged();
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
    if (m_busy || m_commands->busy() || m_demo || m_account.password.isEmpty()) return;
    if (!m_smartMailbox.isEmpty() || !m_search.isEmpty()) { loadCached(append); return; }
    const auto generation = m_generation; const auto account = m_account; const auto folder = m_folder;
    const auto offset = append ? m_offset : 0;
    m_busy = true; if (!quiet) setStatus(tr("Fetching message headers…"));
    m_sync->page(account, folder, offset, pageSize, [this, generation, append, quiet](const Page &page, const QString &error) {
        if (generation != m_generation) return;
        m_busy = false;
        if (!error.isEmpty()) {
            m_cachedOnly = true; loadCached(append);
            if (!quiet) setStatus(tr("Offline · cached mail"), error);
            return;
        }
        m_busy = false;
        if (!m_database->savePage(page)) {
            if (!quiet) setStatus(MailController::tr("Could not cache messages"), m_database->error());
            return;
        }
        m_uidValidity = page.uidValidity;
        const bool changedValidity = !m_content.summary.uidValidity.isEmpty() && m_content.summary.folder == page.folder
            && m_content.summary.uidValidity != page.uidValidity;
        const bool listChangedValidity = m_messages->count() > 0
            && m_messages->get(0).value("uidValidity").toString() != page.uidValidity;
        if (changedValidity || listChangedValidity) {
            ++m_selectionGeneration; m_selected.clear(); m_selectedUid.clear(); m_content = {};
            emit selectionChanged();
        }
        if (append && listChangedValidity) {
            // Offsets refer to the old mailbox generation. The returned
            // tail cannot substitute for the new generation's first page.
            m_messages->clear(); m_offset = 0; m_hasMore = false;
            fetchPage(false, quiet);
            return;
        }
        if (m_unreadOnly) {
            // The network page is intentionally raw. Recompute filtered
            // offsets against the complete cache after saving it.
            loadCached(false);
        } else {
            if (append) m_messages->append(page.messages); else m_messages->replace(page.messages);
            m_offset = page.nextOffset; m_hasMore = page.hasMore;
        }
        m_cachedOnly = m_unreadOnly || !m_search.isEmpty();
        // Saved headers changed the cached unread totals.
        emit folderUnreadChanged();
        if (quiet && !append && page.folder == QLatin1String("INBOX")) checkNewMail(page);
        if (!quiet) setStatus(MailController::tr("Up to date · headers cached"));
    });
}

void MailController::present(const Content &content) {
    m_content = content;
    const auto key = RemoteImageLoader::key(m_account.id, content.summary, content.bodyFormatVersion);
    if (!content.displayHtml.isEmpty()) m_content.displayHtml = m_images->cached(key, content.displayHtml);
    else m_content.html = m_images->cached(key, content.html);
    m_selectedUid = content.summary.uid; m_selected = contentMap(m_content);
    emit selectionChanged();
    loadAllowedImages();
}
int MailController::blockedImageCount() const {
    const auto &html = m_content.displayHtml.isEmpty() ? m_content.html : m_content.displayHtml;
    return int(html.count(QStringLiteral("data-nkmail-src=\"")));
}
bool MailController::remoteImagesLoading() const {
    return m_images->loading(RemoteImageLoader::key(m_account.id, m_content.summary, m_content.bodyFormatVersion));
}
int MailController::failedImageCount() const {
    const auto &html = m_content.displayHtml.isEmpty() ? m_content.html : m_content.displayHtml;
    return int(html.count(QStringLiteral("data-nkmail-failed=\"1\"")));
}
bool MailController::alwaysShowRemoteImages() const { return m_images->alwaysAllow(); }
void MailController::setAlwaysShowRemoteImages(bool allow) {
    if (alwaysShowRemoteImages() == allow) return;
    m_images->setAlwaysAllow(allow); emit imagePolicyChanged(); loadAllowedImages();
}
bool MailController::senderImagesAllowed() const {
    return m_images->senderAllowed(m_account.id, m_content.summary.senderAddress);
}
void MailController::setSenderImagesAllowed(bool allow) {
    m_images->setSenderAllowed(m_account.id, m_content.summary.senderAddress, allow);
    emit selectionChanged(); loadAllowedImages();
}
void MailController::resetImagePermissions() {
    m_images->resetPermissions(); emit imagePolicyChanged(); emit selectionChanged();
}
void MailController::loadAllowedImages() {
    if (!m_selected.value("bodyPending").toBool() && blockedImageCount() > 0
        && m_images->allowed(m_account.id, m_content.summary))
        m_images->load(RemoteImageLoader::key(m_account.id, m_content.summary, m_content.bodyFormatVersion), m_content.displayHtml.isEmpty() ? m_content.html : m_content.displayHtml);
}
void MailController::loadRemoteImages() {
    if (m_selected.value("bodyPending").toBool() || blockedImageCount() == 0) return;
    m_images->allowMessage(m_account.id, m_content.summary);
    m_images->load(RemoteImageLoader::key(m_account.id, m_content.summary, m_content.bodyFormatVersion), m_content.displayHtml.isEmpty() ? m_content.html : m_content.displayHtml);
}
void MailController::openMessage(const QString &uid, const QString &validity, const QString &sourceFolder, const QString &sourceAccount) {
    if (!sourceAccount.isEmpty() && sourceAccount != m_account.id) {
        if (m_smartMailbox != QLatin1String("allInbox")) return;
        // Verify aggregate membership before touching account/credential state.
        if (m_messages->indexOf(sourceFolder, validity, uid, sourceAccount) < 0) return;
        switchAccount(sourceAccount, true);
        if (m_account.id != sourceAccount) return;
    }
    const auto folder = sourceFolder.isEmpty() ? m_folder : sourceFolder;
    if (!m_smartMailbox.isEmpty() && m_messages->indexOf(folder, validity, uid, m_account.id) < 0) return;
    if (m_commands->busy()) return;
    const auto selectionGeneration = ++m_selectionGeneration;
    bool upgrading = false;
    if (auto cached = m_database->content(folder, validity, uid)) {
        present(*cached);
        if (cached->bodyFormatVersion >= CurrentBodyFormatVersion || m_demo || m_account.password.isEmpty()) return;
        upgrading = true; // Re-read only this selected message with PEEK; keep cache visible.
    }
    if (!upgrading) {
        m_selectedUid = uid; m_content = {};
        const int row = m_messages->indexOf(folder, validity, uid, m_account.id);
        if (row >= 0) m_content.summary = summaryFromMap(m_messages->get(row));
        else { m_content.summary.uid = uid; m_content.summary.uidValidity = validity; m_content.summary.folder = folder; }
        m_content.to = m_content.summary.recipients;
        m_selected = contentMap(m_content); m_selected.insert("bodyPending", true);
    }
    m_selected.insert("bodyLoading", !m_demo && !m_account.password.isEmpty());
    m_selected.insert("bodyError", QString{});
    if (m_demo || m_account.password.isEmpty()) {
        m_selected.insert("bodyError", tr("Connect to download its content.")); emit selectionChanged();
        setStatus(tr("This message is not cached"), tr("Connect to download its content.")); return;
    }
    emit selectionChanged();
    const auto account = m_account;
    setStatus(tr("Loading message without changing server flags…"));
    m_sync->content(account, folder, validity, uid, [this, selectionGeneration](const Content &content, const QString &error) {
        // List refresh/filter/flag operations do not change the selected body.
        // Account, folder, selection and completed removal invalidate this token.
        if (selectionGeneration != m_selectionGeneration) return;
        const auto failed = [this](const QString &message) {
            m_selected.insert("bodyLoading", false);
            m_selected.insert("bodyError", QCoreApplication::translate("MailErrors", message.toUtf8().constData()));
            emit selectionChanged();
        };
        if (!error.isEmpty()) { failed(error); setStatus(tr("Could not load message"), error); return; }
        Content current = content;
        // The PEEK snapshot can predate a concurrent read/star update.
        if (sameMessage(m_content.summary, current.summary)) {
            current.summary.unread = m_content.summary.unread;
            current.summary.flagged = m_content.summary.flagged;
        }
        if (!m_database->saveContent(current)) { failed(m_database->error()); setStatus(tr("Could not cache content"), m_database->error()); return; }
        present(current); setStatus(tr("Message cached · remote content blocked"));
    });
}

QVariantMap MailController::newDraft(const QString &mode) { return m_drafts->newDraft(mode, m_account, m_content); }
QVariantMap MailController::loadDraft(const QString &id) { return m_drafts->loadDraft(id); }
QVariantMap MailController::saveDraft(const QVariantMap &draft) { return m_drafts->saveDraft(draft); }
void MailController::discardDraft(const QString &id) { if (!sending() && !sendPending()) m_drafts->discardDraft(id); }
QVariantMap MailController::addAttachment(const QString &url) { return m_drafts->addAttachment(url); }
void MailController::saveAttachment(int index, const QString &url) { m_drafts->saveAttachment(m_content, index, url); }
void MailController::sendDraft(const QVariantMap &draft) {
    if (m_busy || m_commands->busy()) return;
    m_send->sendDraft(m_account, draft);
}
void MailController::cancelSend() { m_send->cancelSend(); }
void MailController::clearPendingSend() { m_send->clearPendingSend(); }
bool MailController::sending() const { return m_send->sending(); }
bool MailController::sendPending() const { return m_send->pending(); }
int MailController::sendDelay() const { return m_send->delay(); }
void MailController::clearError() { m_error.clear(); emit statusChanged(); }
void MailController::disconnectAccount() {
    if (sending()) return;
    m_sync->resetReadConnection();
    clearPendingSend();
    ++m_generation; ++m_selectionGeneration; m_account.password.clear(); m_busy = false; m_restorePending = false; m_switchCredentialPending = false; m_cachedOnly = true;
    m_refreshTimer.stop();
    m_credentials->suspend(m_account);
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue("restoreAllowed", false); settings.sync();
    setStatus(tr("Disconnected · cached mail available"));
}
void MailController::setMessageRead(int index, bool read) { setMessagesRead({index}, read); }
void MailController::toggleFlagged(int index) { setMessagesFlagged({index}, !m_messages->get(index).value("flagged").toBool()); }
void MailController::deleteMessage(int index) { deleteMessages({index}); }
void MailController::archiveMessage(int index) { archiveMessages({index}); }
void MailController::moveMessage(int index, const QString &destination) { moveMessages({index}, destination); }
void MailController::setMessagesRead(const QVariantList &indices, bool read) { executeCommand(indices, MailboxCommands::Action::Read, read); }
void MailController::setMessagesFlagged(const QVariantList &indices, bool flagged) { executeCommand(indices, MailboxCommands::Action::Flag, flagged); }
void MailController::deleteMessages(const QVariantList &indices) { executeCommand(indices, MailboxCommands::Action::Remove); }
void MailController::archiveMessages(const QVariantList &indices) {
    for (const auto &value : m_folders) {
        const auto folder = value.toMap();
        if (isArchiveFolder(m_folders, folder.value("id").toString())) {
            moveMessages(indices, folder.value("id").toString()); return;
        }
    }
    setStatus(tr("Archive folder unavailable"), tr("This account has no Archive, Archives or All Mail folder."));
}
void MailController::moveMessages(const QVariantList &indices, const QString &destination) {
    if (destination.trimmed().isEmpty() || (m_smartMailbox.isEmpty() && destination == m_folder)) {
        setStatus(tr("Choose a different destination folder")); return;
    }
    executeCommand(indices, MailboxCommands::Action::Move, false, destination);
}
void MailController::executeCommand(const QVariantList &indices, MailboxCommands::Action action,
                                     bool value, const QString &destination) {
    if (m_smartMailbox == QLatin1String("allInbox")) {
        setStatus(tr("Open the account folder to change messages.")); return;
    }
    if (m_commands->busy() || m_busy || sending() || sendPending()) { setStatus(tr("Another mail operation is in progress")); return; }
    if (!m_demo && (!configured() || m_account.password.isEmpty())) {
        if (m_account.imapHost == "imap.qq.com") setStatus(tr("Account needs attention"), tr("Reconnect with your QQ Mail authorization code. Enable Remember password to restore it after restarting."));
        else if (m_account.authMethod == "google_oauth") setStatus(tr("Account needs attention"), tr("Enter a Google app password in account settings to reconnect this account."));
        else setStatus(tr("Account needs attention"), tr("Enter an application password before changing messages."));
        return;
    }
    QList<Summary> selected;
    QSet<QString> seen;
    for (const auto &index : indices) {
        const auto map = m_messages->get(index.toInt());
        if (map.isEmpty()) continue;
        const auto summary = summaryFromMap(map);
        if (m_smartMailbox.isEmpty() && (summary.folder != m_folder || summary.uidValidity != m_uidValidity)) continue;
        const auto identity = MessageListModel::identity(summary);
        if (seen.contains(identity)) continue;
        seen.insert(identity); selected.append(summary);
    }
    if (selected.isEmpty()) return;
    ++m_generation; m_serverSearching = false;
    setStatus(tr("Updating messages…"));
    m_commands->execute(m_account, selected, action, value, destination, m_demo);
    emit statusChanged();
}
}
