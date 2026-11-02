#include "DraftService.h"
#include "storage/MailDatabase.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUuid>
namespace nkmail {
namespace {
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
QVariantMap DraftService::newDraft(const QString &mode, const Account &account, const Content &content) {
    if (mode != "new" && mode != "reply" && mode != "replyAll" && mode != "forward") {
        setStatus(tr("Could not create draft"), tr("Choose new, reply, reply all or forward.")); return {};
    }
    if (mode != "new" && content.summary.uid.isEmpty()) {
        setStatus(tr("Could not create draft"), tr("Select a message before replying or forwarding.")); return {};
    }
    Draft draft; draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces); draft.updated = QDateTime::currentDateTimeUtc();
    if (mode != "new") {
        const auto &s = content.summary;
        draft.subject = cleanSubject(mode == "forward" ? "Fwd: " : "Re: ", s.subject);
        if (mode != "forward") {
            QStringList primary;
            if (!replyAddresses(content.replyTo.isEmpty() ? s.senderAddress : content.replyTo, &primary) || primary.isEmpty()) {
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
                const QString self = account.address.trimmed().toCaseFolded();
                recipients.removeIf([&self](const QString &address) { return address.toCaseFolded() == self; });
                draft.to = recipients.join(", ");
                seen.insert(self);
                QStringList others;
                for (const auto &field : {content.to, content.cc}) {
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
            draft.inReplyTo = content.messageId; draft.references = content.messageId;
        }
        draft.body = QStringLiteral("\n\n— %1 · %2 —\n").arg(s.sender, s.date);
        for (const auto &line : content.text.split('\n')) draft.body += "> " + line + '\n';
        QString quoted = content.html;
        if (quoted.isEmpty()) {
            quoted = escapeHtml(content.text);
            quoted.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        }
        draft.html = QStringLiteral("<p></p><p>— %1 · %2 —</p><blockquote>%3</blockquote>")
                         .arg(escapeHtml(s.sender), escapeHtml(s.date), quoted);
        if (mode == "forward") for (const auto &attachment : content.attachments) {
            const auto path = QDir(m_root).filePath("attachments/" + QUuid::createUuid().toString(QUuid::WithoutBraces));
            QDir().mkpath(QFileInfo(path).absolutePath()); QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(attachment.bytes) != attachment.bytes.size() || !file.commit()) continue;
            QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            auto copy = attachment; copy.path = path; copy.bytes.clear(); draft.attachments.append(copy);
        }
    }
    if (!m_database->saveDraft(draft)) { setStatus(tr("Could not save draft"), m_database->error()); return {}; }
    emit draftsChanged(); return loadDraft(draft.id);
}
QVariantMap DraftService::loadDraft(const QString &id) {
    for (const auto &draft : m_database->drafts()) if (draft.id == id) { auto result = draftMap(draft); result["accountId"] = m_database->accountId(); return result; }
    return {};
}
QVariantMap DraftService::saveDraft(const QVariantMap &map) {
    if (map.contains("accountId") && map.value("accountId").toString() != m_database->accountId()) {
        setStatus(tr("Could not save draft"), tr("This draft belongs to another account. Switch back before editing.")); return {};
    }
    Draft draft = draftFromMap(map);
    if (draft.id.isEmpty()) draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft.updated = QDateTime::currentDateTimeUtc();
    if (!m_database->saveDraft(draft)) { setStatus(tr("Could not save draft"), m_database->error()); return {}; }
    emit draftsChanged();
    return loadDraft(draft.id);
}
void DraftService::discardDraft(const QString &id) {
    if (!m_database->deleteDraft(id)) setStatus(tr("Could not remove local draft"), m_database->error());
    emit draftsChanged();
}
QVariantMap DraftService::addAttachment(const QString &url) {
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
void DraftService::saveAttachment(const Content &content, int index, const QString &url) {
    if (index < 0 || index >= content.attachments.size()) return;
    const auto path = localPath(url);
    if (path.isEmpty() || QFileInfo::exists(path)) { setStatus(tr("Choose a new file name"), tr("Existing files are not overwritten.")); return; }
    QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { setStatus(tr("Could not save attachment")); return; }
    const auto &bytes = content.attachments[index].bytes;
    if (file.write(bytes) != bytes.size()) { file.close(); file.remove(); setStatus(tr("Could not save complete attachment")); return; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner); file.close(); setStatus(tr("Attachment saved"));
}
}
