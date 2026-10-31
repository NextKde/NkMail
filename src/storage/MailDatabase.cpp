#include "MailDatabase.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>

namespace nkmail {
namespace {
constexpr int SchemaVersion = 3;
QString json(const QVariantMap &map)
{
    return QString::fromUtf8(QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact));
}
QVariantMap mapFromJson(const QVariant &value)
{
    return QJsonDocument::fromJson(value.toString().toUtf8()).object().toVariantMap();
}
Summary summaryFromMap(const QVariantMap &m)
{
    Summary s;
    s.folder = m.value("folder").toString();
    s.uidValidity = m.value("uidValidity").toString();
    s.uid = m.value("uid").toString();
    s.sender = m.value("sender").toString();
    s.senderAddress = m.value("senderAddress").toString();
    s.recipients = m.value("recipients").toString();
    s.subject = m.value("subject").toString();
    s.date = m.value("date").toString();
    s.preview = m.value("preview").toString();
    s.messageId = m.value("messageId").toString();
    s.inReplyTo = m.value("inReplyTo").toString();
    s.references = m.value("references").toString();
    s.unread = m.value("unread").toBool();
    s.flagged = m.value("flagged").toBool();
    s.hasAttachments = m.value("hasAttachments").toBool();
    return s;
}
bool validIdentity(const Summary &s)
{
    return !s.folder.isEmpty() && !s.uidValidity.isEmpty() && !s.uid.isEmpty();
}
bool exec(QSqlDatabase db, const QString &sql, const QVariantList &values, QString *error)
{
    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        *error = q.lastError().text();
        return false;
    }
    for (const auto &value : values)
        q.addBindValue(value);
    if (q.exec())
        return true;
    *error = q.lastError().text();
    return false;
}
class Transaction {
public:
    explicit Transaction(QSqlDatabase db) : m_db(db), m_active(db.transaction()) {}
    ~Transaction() { if (m_active) m_db.rollback(); }
    bool active() const { return m_active; }
    bool commit()
    {
        if (!m_active || !m_db.commit())
            return false;
        m_active = false;
        return true;
    }
private:
    QSqlDatabase m_db;
    bool m_active;
};
}
MailDatabase::MailDatabase() : m_thread(QThread::currentThread()) {}
MailDatabase::~MailDatabase() { close(); }
bool MailDatabase::fail(const QString &message) const
{
    m_error = message;
    return false;
}
bool MailDatabase::check() const
{
    if (QThread::currentThread() != m_thread)
        return fail(QStringLiteral("MailDatabase must be used on its owning thread"));
    m_error.clear();
    if (!m_db.isValid() || !m_db.isOpen())
        return fail(QStringLiteral("Mail database is not open"));
    return true;
}
bool MailDatabase::isOpen() const
{
    return QThread::currentThread() == m_thread && m_db.isValid() && m_db.isOpen();
}
void MailDatabase::close()
{
    if (QThread::currentThread() != m_thread) {
        fail(QStringLiteral("MailDatabase must be closed on its owning thread"));
        return;
    }
    if (m_db.isValid())
        m_db.close();
    m_db = QSqlDatabase();
    if (!m_connection.isEmpty())
        QSqlDatabase::removeDatabase(m_connection);
    m_connection.clear();
}
bool MailDatabase::open(const QString &root)
{
    if (QThread::currentThread() != m_thread)
        return fail(QStringLiteral("MailDatabase must be opened on its owning thread"));
    close();
    m_error.clear();
    if (root.trimmed().isEmpty())
        return fail(QStringLiteral("An explicit storage root is required"));
    QDir dir(root);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return fail(QStringLiteral("Cannot create storage root"));
    const QString filename = dir.absoluteFilePath(QStringLiteral("mail.sqlite"));
    // SQLite may create WAL/SHM files beside the database; keep its directory private.
    if (!QFile::setPermissions(dir.absolutePath(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
        return fail(QStringLiteral("Cannot protect storage root permissions"));
    m_connection = QStringLiteral("nkmail-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    m_db.setDatabaseName(filename);
    m_db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!m_db.open()) {
        const QString message = m_db.lastError().text();
        close();
        return fail(message);
    }
    if (!QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner)
        || !exec(m_db, QStringLiteral("PRAGMA foreign_keys=ON"), {}, &m_error)
        || !exec(m_db, QStringLiteral("PRAGMA journal_mode=WAL"), {}, &m_error)
        || !exec(m_db, QStringLiteral("PRAGMA synchronous=FULL"), {}, &m_error)
        || !migrate()
        // A crash during SMTP submission has an ambiguous outcome. Never retry it.
        || !exec(m_db, QStringLiteral("UPDATE drafts SET state='unknown' WHERE state='sending'"), {}, &m_error)) {
        const QString message = m_error.isEmpty() ? QStringLiteral("Cannot protect database permissions") : m_error;
        close();
        return fail(message);
    }
    return true;
}
bool MailDatabase::migrate()
{
    QSqlQuery versionQuery(m_db);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version")) || !versionQuery.next())
        return fail(versionQuery.lastError().text());
    const int version = versionQuery.value(0).toInt();
    versionQuery.finish();
    if (version < 0 || version > SchemaVersion)
        return fail(QStringLiteral("Unsupported mail database schema version %1").arg(version));
    Transaction tx(m_db);
    if (!tx.active())
        return fail(m_db.lastError().text());
    if (version < 1) {
        const QStringList schema = {
            QStringLiteral("CREATE TABLE accounts (slot INTEGER PRIMARY KEY CHECK(slot=1), payload TEXT NOT NULL)"),
            QStringLiteral("CREATE TABLE folders (id TEXT PRIMARY KEY, title TEXT NOT NULL, total INTEGER NOT NULL, position INTEGER NOT NULL)"),
            QStringLiteral("CREATE TABLE folder_state (folder TEXT PRIMARY KEY, uid_validity TEXT NOT NULL)"),
            QStringLiteral("CREATE TABLE summaries (folder TEXT NOT NULL, uid_validity TEXT NOT NULL, uid TEXT NOT NULL, payload TEXT NOT NULL, PRIMARY KEY(folder,uid_validity,uid))"),
            QStringLiteral("CREATE TABLE contents (folder TEXT NOT NULL, uid_validity TEXT NOT NULL, uid TEXT NOT NULL, payload TEXT NOT NULL, PRIMARY KEY(folder,uid_validity,uid))"),
            QStringLiteral("CREATE TABLE drafts (id TEXT PRIMARY KEY, payload TEXT NOT NULL)")
        };
        for (const auto &sql : schema)
            if (!exec(m_db, sql, {}, &m_error))
                return false;
    }
    if (version < 2) {
        if (!exec(m_db, QStringLiteral("ALTER TABLE drafts ADD COLUMN state TEXT NOT NULL DEFAULT 'draft'"), {}, &m_error)
            || !exec(m_db, QStringLiteral("ALTER TABLE drafts ADD COLUMN updated TEXT NOT NULL DEFAULT ''"), {}, &m_error))
            return false;
        // Migrate older JSON drafts without losing send state or timestamps.
        QSqlQuery q(m_db);
        if (!q.exec(QStringLiteral("SELECT id,payload FROM drafts")))
            return fail(q.lastError().text());
        QList<Draft> oldDrafts;
        while (q.next()) {
            auto draft = draftFromMap(mapFromJson(q.value(1)));
            draft.id = q.value(0).toString();
            oldDrafts.append(draft);
        }
        q.finish();
        for (const auto &d : oldDrafts)
            if (!exec(m_db, QStringLiteral("UPDATE drafts SET state=?,updated=? WHERE id=?"),
                      {d.state, d.updated.toUTC().toString(Qt::ISODateWithMs), d.id}, &m_error))
                return false;
        if (!exec(m_db, QStringLiteral("CREATE INDEX drafts_updated ON drafts(updated DESC,id)"), {}, &m_error))
            return false;
    }
    if (version < 3) {
        if (!exec(m_db, QStringLiteral("CREATE TABLE retired_validities (folder TEXT NOT NULL, uid_validity TEXT NOT NULL, PRIMARY KEY(folder,uid_validity))"), {}, &m_error))
            return false;
    }
    if (!exec(m_db, QStringLiteral("PRAGMA user_version=3"), {}, &m_error))
        return false;
    return tx.commit() || fail(m_db.lastError().text());
}
Account MailDatabase::account() const
{
    if (!check())
        return {};
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT payload FROM accounts WHERE slot=1"))) {
        fail(q.lastError().text());
        return {};
    }
    if (!q.next())
        return {};
    Account a = accountFromMap(mapFromJson(q.value(0)));
    a.password.clear();
    return a;
}
bool MailDatabase::saveAccount(const Account &a)
{
    if (!check())
        return false;
    Transaction tx(m_db);
    if (!tx.active())
        return fail(m_db.lastError().text());
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT payload FROM accounts WHERE slot=1")))
        return fail(q.lastError().text());
    if (q.next()) {
        const Account previous = accountFromMap(mapFromJson(q.value(0)));
        const bool differentMailbox = previous.id != a.id || previous.address != a.address
            || previous.username != a.username || previous.imapHost != a.imapHost
            || previous.imapPort != a.imapPort || previous.imapStartTls != a.imapStartTls;
        q.finish();
        if (differentMailbox) {
            // Folder/UID identity is scoped to an account, never just a server UID.
            // Keep unsent local drafts, but never show another account's cache.
            for (const auto &table : {"contents", "summaries", "folder_state", "retired_validities", "folders"})
                if (!exec(m_db, QStringLiteral("DELETE FROM ") + QLatin1String(table), {}, &m_error))
                    return false;
        }
    }
    q.finish();
    if (!exec(m_db, QStringLiteral("INSERT INTO accounts(slot,payload) VALUES(1,?) ON CONFLICT(slot) DO UPDATE SET payload=excluded.payload"),
              {json(accountMap(a))}, &m_error))
        return false;
    return tx.commit() || fail(m_db.lastError().text());
}
QList<Folder> MailDatabase::folders() const
{
    QList<Folder> result;
    if (!check())
        return result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT id,title,total FROM folders ORDER BY position,id"))) {
        fail(q.lastError().text());
        return result;
    }
    while (q.next())
        result.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toInt()});
    return result;
}
bool MailDatabase::saveFolders(const QList<Folder> &folders)
{
    if (!check())
        return false;
    Transaction tx(m_db);
    if (!tx.active())
        return fail(m_db.lastError().text());
    if (!exec(m_db, QStringLiteral("DELETE FROM folders"), {}, &m_error))
        return false;
    int position = 0;
    for (const auto &f : folders) {
        if (f.id.isEmpty())
            return fail(QStringLiteral("A folder ID is required"));
        if (!exec(m_db, QStringLiteral("INSERT INTO folders(id,title,total,position) VALUES(?,?,?,?)"),
                  {f.id, f.title.isNull() ? QStringLiteral("") : f.title, f.total, position++}, &m_error))
            return false;
    }
    return tx.commit() || fail(m_db.lastError().text());
}
QString MailDatabase::validity(const QString &folder) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT uid_validity FROM folder_state WHERE folder=?"));
    q.addBindValue(folder);
    if (!q.exec()) {
        fail(q.lastError().text());
        return {};
    }
    return q.next() ? q.value(0).toString() : QString();
}
bool MailDatabase::setValidity(const QString &folder, const QString &uidValidity)
{
    if (folder.isEmpty() || uidValidity.isEmpty())
        return fail(QStringLiteral("Folder and UIDVALIDITY are required"));
    QSqlQuery retired(m_db);
    retired.prepare(QStringLiteral("SELECT 1 FROM retired_validities WHERE folder=? AND uid_validity=?"));
    retired.addBindValue(folder);
    retired.addBindValue(uidValidity);
    if (!retired.exec())
        return fail(retired.lastError().text());
    if (retired.next())
        return fail(QStringLiteral("UIDVALIDITY belongs to a retired generation"));
    retired.finish();
    const QString previous = validity(folder);
    if (!m_error.isEmpty())
        return false;
    if (!previous.isEmpty() && previous != uidValidity
        && !exec(m_db, QStringLiteral("INSERT OR IGNORE INTO retired_validities(folder,uid_validity) VALUES(?,?)"),
                 {folder, previous}, &m_error))
        return false;
    // This runs inside the caller's transaction. Old bodies and summaries are
    // purged atomically with the new generation, even if a UID is reused.
    if (!exec(m_db, QStringLiteral("DELETE FROM contents WHERE folder=? AND uid_validity<>?"),
              {folder, uidValidity}, &m_error)
        || !exec(m_db, QStringLiteral("DELETE FROM summaries WHERE folder=? AND uid_validity<>?"),
                 {folder, uidValidity}, &m_error))
        return false;
    return exec(m_db, QStringLiteral("INSERT INTO folder_state(folder,uid_validity) VALUES(?,?) ON CONFLICT(folder) DO UPDATE SET uid_validity=excluded.uid_validity"),
                {folder, uidValidity}, &m_error);
}
bool MailDatabase::invalidateUidValidity(const QString &folder, const QString &uidValidity)
{
    if (!check())
        return false;
    Transaction tx(m_db);
    if (!tx.active())
        return fail(m_db.lastError().text());
    if (!setValidity(folder, uidValidity))
        return false;
    return tx.commit() || fail(m_db.lastError().text());
}
bool MailDatabase::savePage(const Page &page)
{
    if (!check())
        return false;
    for (const auto &s : page.messages)
        if (!validIdentity(s) || s.folder != page.folder || s.uidValidity != page.uidValidity)
            return fail(QStringLiteral("Page contains an invalid or mismatched message identity"));
    Transaction tx(m_db);
    if (!tx.active())
        return fail(m_db.lastError().text());
    if (!setValidity(page.folder, page.uidValidity))
        return false;
    for (const auto &s : page.messages)
        if (!exec(m_db, QStringLiteral("INSERT INTO summaries(folder,uid_validity,uid,payload) VALUES(?,?,?,?) ON CONFLICT(folder,uid_validity,uid) DO UPDATE SET payload=excluded.payload"),
                  {s.folder, s.uidValidity, s.uid, json(summaryMap(s))}, &m_error))
            return false;
    return tx.commit() || fail(m_db.lastError().text());
}
Page MailDatabase::cachedPage(const QString &folder, int offset, int limit, const QString &search, bool unreadOnly) const
{
    Page page;
    page.folder = folder;
    if (!check())
        return page;
    if (offset < 0 || limit <= 0 || limit > 10000) {
        fail(QStringLiteral("Invalid cache page bounds"));
        return page;
    }
    page.uidValidity = validity(folder);
    if (!m_error.isEmpty() || page.uidValidity.isEmpty())
        return page;
    QSqlQuery q(m_db);
    const bool filtered = unreadOnly || !search.isEmpty();
    QString sql = QStringLiteral("SELECT payload FROM summaries WHERE folder=? AND uid_validity=? ORDER BY length(uid) DESC,uid DESC");
    if (!filtered)
        sql += QStringLiteral(" LIMIT ? OFFSET ?");
    q.prepare(sql);
    q.addBindValue(folder);
    q.addBindValue(page.uidValidity);
    if (!filtered) {
        q.addBindValue(limit + 1);
        q.addBindValue(offset);
    }
    if (!q.exec()) {
        fail(q.lastError().text());
        return page;
    }
    int matched = 0;
    while (q.next()) {
        const auto s = summaryFromMap(mapFromJson(q.value(0)));
        if (unreadOnly && !s.unread)
            continue;
        if (!search.isEmpty()) {
            const QString haystack = s.sender + '\n' + s.senderAddress + '\n' + s.recipients
                                   + '\n' + s.subject + '\n' + s.preview;
            if (!haystack.contains(search, Qt::CaseInsensitive))
                continue;
        }
        // Filter the entire cache before counting offsets, including the extra
        // matching row used for hasMore. Raw mailbox offsets are unrelated.
        if (filtered && matched++ < offset)
            continue;
        if (page.messages.size() == limit) {
            page.hasMore = true;
            break;
        }
        page.messages.append(s);
    }
    page.nextOffset = offset + int(page.messages.size());
    return page;
}
bool MailDatabase::saveContent(const Content &content)
{
    if (!check())
        return false;
    const auto &s = content.summary;
    if (!validIdentity(s))
        return fail(QStringLiteral("Content requires a complete message identity"));
    const QString current = validity(s.folder);
    if (!m_error.isEmpty())
        return false;
    // A late content callback must never roll the folder back to an old generation.
    if (!current.isEmpty() && current != s.uidValidity)
        return fail(QStringLiteral("Content UIDVALIDITY is stale"));
    Transaction tx(m_db);
    if (!tx.active())
        return fail(m_db.lastError().text());
    if (!setValidity(s.folder, s.uidValidity))
        return false;
    auto payload = contentMap(content);
    QVariantList attachments;
    for (const auto &a : content.attachments) {
        attachments.append(QVariantMap{{"name", a.name}, {"mimeType", a.mimeType},
                                       {"path", a.path}, {"size", QString::number(a.size)},
                                       {"bytes", QString::fromLatin1(a.bytes.toBase64())}});
    }
    payload.insert("attachments", attachments);
    if (!exec(m_db, QStringLiteral("INSERT INTO contents(folder,uid_validity,uid,payload) VALUES(?,?,?,?) ON CONFLICT(folder,uid_validity,uid) DO UPDATE SET payload=excluded.payload"),
              {s.folder, s.uidValidity, s.uid, json(payload)}, &m_error))
        return false;
    return tx.commit() || fail(m_db.lastError().text());
}
std::optional<Content> MailDatabase::content(const QString &folder, const QString &uidValidity, const QString &uid) const
{
    if (!check())
        return std::nullopt;
    if (validity(folder) != uidValidity || !m_error.isEmpty())
        return std::nullopt;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT payload FROM contents WHERE folder=? AND uid_validity=? AND uid=?"));
    q.addBindValue(folder);
    q.addBindValue(uidValidity);
    q.addBindValue(uid);
    if (!q.exec()) {
        fail(q.lastError().text());
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;
    const auto m = mapFromJson(q.value(0));
    Content c;
    c.summary = summaryFromMap(m.value("summary").toMap());
    c.text = m.value("text").toString();
    c.messageId = m.value("messageId").toString();
    c.replyTo = m.value("replyTo").toString();
    c.to = m.value("to").toString();
    c.cc = m.value("cc").toString();
    for (const auto &value : m.value("attachments").toList()) {
        const auto a = value.toMap();
        c.attachments.append({a.value("name").toString(), a.value("mimeType").toString(),
                              a.value("path").toString(), a.value("size").toLongLong(),
                              QByteArray::fromBase64(a.value("bytes").toString().toLatin1())});
    }
    return c;
}
bool MailDatabase::removeMessage(const QString &folder, const QString &uidValidity, const QString &uid)
{
    if (!check())
        return false;
    if (folder.isEmpty() || uidValidity.isEmpty() || uid.isEmpty())
        return fail(QStringLiteral("Folder, UIDVALIDITY and UID are required"));
    const QVariantList identity{folder, uidValidity, uid};
    return exec(m_db, QStringLiteral("DELETE FROM contents WHERE folder=? AND uid_validity=? AND uid=?"), identity, &m_error)
        && exec(m_db, QStringLiteral("DELETE FROM summaries WHERE folder=? AND uid_validity=? AND uid=?"), identity, &m_error);
}
QList<Draft> MailDatabase::drafts() const
{
    QList<Draft> result;
    if (!check())
        return result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT id,payload,state,updated FROM drafts ORDER BY updated DESC,id"))) {
        fail(q.lastError().text());
        return result;
    }
    while (q.next()) {
        auto draft = draftFromMap(mapFromJson(q.value(1)));
        draft.id = q.value(0).toString();
        draft.state = q.value(2).toString();
        draft.updated = QDateTime::fromString(q.value(3).toString(), Qt::ISODateWithMs);
        result.append(draft);
    }
    return result;
}
bool MailDatabase::saveDraft(const Draft &draft)
{
    if (!check())
        return false;
    if (draft.id.isEmpty())
        return fail(QStringLiteral("A draft ID is required"));
    Transaction tx(m_db);
    if (!tx.active())
        return fail(m_db.lastError().text());
    auto d = draft;
    if (d.state.isEmpty())
        d.state = QStringLiteral("draft");
    QSqlQuery existing(m_db);
    existing.prepare(QStringLiteral("SELECT state FROM drafts WHERE id=?"));
    existing.addBindValue(d.id);
    if (!existing.exec())
        return fail(existing.lastError().text());
    if (existing.next()) {
        const QString persisted = existing.value(0).toString();
        // The UI often holds a stale draft map during/after an asynchronous send.
        // Only markDraftState may finalize an in-flight submission. Autosave may
        // update text, but cannot make a protected draft eligible for submission.
        if (persisted == QLatin1String("sending") || persisted == QLatin1String("accepted")
            || persisted == QLatin1String("unknown"))
            d.state = persisted;
    }
    existing.finish();
    if (!d.updated.isValid())
        d.updated = QDateTime::currentDateTimeUtc();
    if (!exec(m_db, QStringLiteral("INSERT INTO drafts(id,payload,state,updated) VALUES(?,?,?,?) ON CONFLICT(id) DO UPDATE SET payload=excluded.payload,state=excluded.state,updated=excluded.updated"),
              {d.id, json(draftMap(d)), d.state, d.updated.toUTC().toString(Qt::ISODateWithMs)}, &m_error))
        return false;
    return tx.commit() || fail(m_db.lastError().text());
}
bool MailDatabase::deleteDraft(const QString &id)
{
    if (!check())
        return false;
    return exec(m_db, QStringLiteral("DELETE FROM drafts WHERE id=?"), {id}, &m_error);
}
bool MailDatabase::markDraftState(const QString &id, const QString &state)
{
    if (!check())
        return false;
    if (state.isEmpty())
        return fail(QStringLiteral("A draft state is required"));
    QSqlQuery q(m_db);
    // Compare-and-set in SQL: sending may only finish, accepted/unknown may never
    // become sendable again. Starting requires draft/rejected, not sending.
    q.prepare(QStringLiteral("UPDATE drafts SET state=?,updated=? WHERE id=? AND ("
                             "(state IN ('draft','rejected') AND ? IN ('draft','rejected','sending','unknown','accepted')) OR "
                             "(state='sending' AND ? IN ('accepted','rejected','unknown')) OR "
                             "(state IN ('accepted','unknown') AND state=?))"));
    q.addBindValue(state);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    q.addBindValue(id);
    q.addBindValue(state);
    q.addBindValue(state);
    q.addBindValue(state);
    if (!q.exec())
        return fail(q.lastError().text());
    return q.numRowsAffected() > 0 || fail(QStringLiteral("Draft not found or send state transition is unsafe"));
}
}
