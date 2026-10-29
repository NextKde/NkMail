#include "MailController.h"
#include "MessageListModel.h"
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
}
MailController::MailController(bool demo, const QString &dataRoot, QObject *parent)
    : QObject(parent), m_demo(demo), m_root(dataRoot), m_database(std::make_unique<MailDatabase>()),
      m_messages(new MessageListModel(this)), m_credentials(new CredentialStore(this)) {
    m_pool.setMaxThreadCount(1);
    m_pool.setExpiryTimeout(-1);
    connect(m_credentials, &CredentialStore::passwordReady, this, [this](const QString &id, const QString &password) {
        if (id != credentialKey() || m_demo || !m_restorePending || !m_account.password.isEmpty()) return;
        m_restorePending = false;
        m_account.password = password;
        setStatus(tr("Saved credentials restored. Refresh to connect."));
    });
    connect(m_credentials, &CredentialStore::passwordSaved, this, [this](const QString &) {
        QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
        if (!settings.value("restoreAllowed", false).toBool()) return;
        if (!m_busy && !m_sending) setStatus(tr("Credentials saved in the system keychain."));
    });
    connect(m_credentials, &CredentialStore::error, this, [this](const QString &, const QString &) {
        m_restorePending = false;
        setStatus(tr("System keychain needs attention"), tr("Credential storage or removal could not be completed. The local restore permission is authoritative; no plaintext fallback is used."));
    });
    QTimer::singleShot(0, this, &MailController::initialize);
}
MailController::~MailController() { m_pool.clear(); m_pool.waitForDone(); }
QObject *MailController::messages() const { return m_messages; }
bool MailController::configured() const { return !m_account.address.isEmpty() && !m_account.imapHost.isEmpty(); }
QVariantMap MailController::account() const { return accountMap(m_account); }
QVariantList MailController::drafts() const {
    QVariantList out;
    if (m_database->isOpen()) for (const auto &draft : m_database->drafts()) out.append(draftMap(draft));
    return out;
}
void MailController::setStatus(const QString &status, const QString &error) {
    m_status = status; m_error = error; emit statusChanged();
}
QString MailController::credentialKey() const {
    // Account id is "default" in every profile. Scope secrets to this explicit
    // data root; never probe the legacy global key shared by unrelated profiles.
    const QFileInfo root(m_root);
    QString path = root.canonicalFilePath();
    if (path.isEmpty()) path = root.absoluteFilePath();
    const QByteArray scope = path.toUtf8() + '\0' + m_account.id.toUtf8();
    return QStringLiteral("profile-") + QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
}
void MailController::run(std::function<void()> task) { (void)QtConcurrent::run(&m_pool, std::move(task)); }
void MailController::initialize() {
    if (!m_database->open(m_root)) { setStatus(tr("Storage unavailable"), m_database->error()); return; }
    if (m_demo) { seedDemo(); return; }
    m_account = m_database->account();
    for (const auto &folder : m_database->folders()) m_folders.append(QVariantMap{{"id", folder.id}, {"title", folder.title}, {"total", folder.total}});
    emit accountChanged(); emit foldersChanged(); emit draftsChanged();
    loadCached();
    setStatus(configured() ? tr("Offline · cached mail") : tr("Add your email account to get started"));
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
    // A cache is bound to one account. Changing identity must not expose the previous account's mail.
    if (configured() && (next.address.compare(m_account.address, Qt::CaseInsensitive) != 0 || next.imapHost != m_account.imapHost || next.username != m_account.username)) {
        setStatus(tr("Account change requires a separate data directory"), tr("This first version keeps one mailbox per data directory. Use a new --data-dir for a different account.")); return;
    }
    next.password = password.isEmpty() ? m_account.password : password;
    next.id = QStringLiteral("default");
    const bool changedMailbox = m_account.id != next.id || m_account.address != next.address
        || m_account.username != next.username || m_account.imapHost != next.imapHost
        || m_account.imapPort != next.imapPort || m_account.imapStartTls != next.imapStartTls;
    if (!m_database->saveAccount(next)) { setStatus(tr("Could not save account"), m_database->error()); return; }
    m_restorePending = false;
    m_account = next; ++m_generation; ++m_selectionGeneration;
    if (changedMailbox) {
        m_cachedOnly = true; m_offset = 0; m_hasMore = false;
        m_messages->clear(); m_folders.clear(); m_selected.clear(); m_selectedUid.clear(); m_content = {};
        emit foldersChanged(); emit selectionChanged();
    }
    emit accountChanged();
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue("restoreAllowed", remember && !m_account.password.isEmpty()); settings.sync();
    if (remember && !m_account.password.isEmpty()) m_credentials->write(credentialKey(), m_account.password);
    else m_credentials->remove(credentialKey());
    setStatus(tr("Account saved. Refresh to connect."));
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
    auto page = m_database->cachedPage(m_folder, append ? m_offset : 0, pageSize, m_search);
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
    ++m_generation; m_busy = false; m_search = query; m_offset = 0; emit searchChanged(); loadCached();
    setStatus(m_demo ? tr("Preview · search synthetic cached messages") : tr("Search · cached messages only"));
}
void MailController::loadMore() {
    if (m_busy || !m_hasMore) return;
    if (m_demo || m_cachedOnly || !m_search.isEmpty() || m_account.password.isEmpty()) loadCached(true);
    else fetchPage(true);
}
void MailController::fetchPage(bool append) {
    if (m_busy || m_demo || m_account.password.isEmpty()) return;
    if (!m_search.isEmpty()) { loadCached(append); return; }
    const auto generation = m_generation; const auto account = m_account; const auto folder = m_folder;
    const auto offset = append ? m_offset : 0;
    m_busy = true; setStatus(tr("Fetching message headers…"));
    QPointer<MailController> self(this);
    run([self, account, folder, offset, generation, append] {
        try {
            auto page = MailioBackend::page(account, folder, offset, pageSize);
            if (self) QMetaObject::invokeMethod(self, [self, page, generation, append] {
                if (!self || generation != self->m_generation) return;
                self->m_busy = false;
                if (!self->m_database->savePage(page)) { self->setStatus(self->tr("Could not cache messages"), self->m_database->error()); return; }
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
                    self->fetchPage(false);
                    return;
                }
                if (append) self->m_messages->append(page.messages); else self->m_messages->replace(page.messages);
                self->m_cachedOnly = false;
                self->m_offset = page.nextOffset; self->m_hasMore = page.hasMore;
                self->setStatus(self->tr("Up to date · headers cached · server flags are read-only"));
            }, Qt::QueuedConnection);
        } catch (const std::exception &e) {
            const auto error = QString::fromUtf8(e.what());
            if (self) QMetaObject::invokeMethod(self, [self, generation, error, append] {
                if (!self || generation != self->m_generation) return;
                self->m_busy = false; self->m_cachedOnly = true;
                self->loadCached(append); self->setStatus(self->tr("Offline · cached mail"), error);
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
        try { result = MailioBackend::send(account, draft); }
        catch (...) { result.state = SendResult::Unknown; result.message = QStringLiteral("Connection ended before the result could be confirmed."); }
        if (self) QMetaObject::invokeMethod(self, [self, result, id = draft.id] {
            if (!self) return;
            self->m_sending = false;
            const QString state = result.state == SendResult::Accepted ? "accepted" : result.state == SendResult::Rejected ? "rejected" : "unknown";
            self->m_database->markDraftState(id, state); emit self->draftsChanged();
            if (result.state == SendResult::Accepted) {
                self->setStatus(self->tr("SMTP server accepted this message · delivery not confirmed")); emit self->draftAccepted(id);
            } else self->setStatus(result.state == SendResult::Unknown ? self->tr("Send result unknown · not retried") : self->tr("Message not accepted · draft preserved"), result.message);
        }, Qt::QueuedConnection);
    });
}
void MailController::clearError() { m_error.clear(); emit statusChanged(); }
void MailController::disconnectAccount() {
    if (m_sending) return;
    ++m_generation; ++m_selectionGeneration; m_pool.clear(); m_account.password.clear(); m_busy = false; m_restorePending = false; m_cachedOnly = true;
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue("restoreAllowed", false); settings.sync();
    setStatus(tr("Disconnected · cached mail available"));
}
}
