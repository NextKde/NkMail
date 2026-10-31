#include "MailTypes.h"
#include "MessageListModel.h"
#include "FolderListModel.h"
#include "storage/MailDatabase.h"
#include "storage/CredentialStore.h"
#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

using namespace nkmail;
namespace {
QString temporaryTemplate()
{
    return QStringLiteral(NKMAIL_TEST_TMP_ROOT "/case-XXXXXX");
}
Summary message(QString uid, QString generation = QStringLiteral("100"), QString folder = QStringLiteral("INBOX"))
{
    Summary s;
    s.folder = folder;
    s.uidValidity = generation;
    s.uid = uid;
    s.sender = QStringLiteral("Synthetic Sender");
    s.senderAddress = QStringLiteral("sender@example.invalid");
    s.subject = QStringLiteral("Synthetic message ") + uid;
    s.date = QStringLiteral("2026-10-29T12:00:00Z");
    s.preview = QStringLiteral("Local fixture only");
    return s;
}
Page page(QList<Summary> messages, QString generation = QStringLiteral("100"))
{
    Page p;
    p.folder = QStringLiteral("INBOX");
    p.uidValidity = generation;
    p.messages = messages;
    return p;
}
}
class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void mapRoundTrips();
    void accountAndFolderPersistence();
    void accountChangeClearsMailboxCache();
    void draftRestartAndStates();
    void authoritativeDraftSendStates();
    void uidValidityIsolation();
    void paginationAndSearch();
    void unreadPaginationAndSearch();
    void unreadUidValidityIsolation();
    void migrations();
    void futureSchemaRejected();
    void independentConnectionsAndErrors();
    void messageModelIdentity();
    void folderModelIdentity();
    void modelMutationStress();
    void credentialConstructionDoesNotAccessBackend();
};
void CoreTests::initTestCase()
{
    QVERIFY(QDir().mkpath(QStringLiteral(NKMAIL_TEST_TMP_ROOT)));
}
void CoreTests::mapRoundTrips()
{
    Account a;
    a.address = QStringLiteral("test@example.invalid");
    a.password = QStringLiteral("NEVER-SERIALIZE-THIS-SYNTHETIC-SECRET");
    a.caFile = QStringLiteral("/isolated/test-ca.pem");
    a.imapStartTls = true;
    const auto accountValues = accountMap(a);
    QVERIFY(!accountValues.contains("password"));
    QVERIFY(!QJsonDocument::fromVariant(accountValues).toJson().contains(a.password.toUtf8()));
    const auto restored = accountFromMap(accountValues);
    QCOMPARE(restored.address, a.address);
    QCOMPARE(restored.imapPort, 993);
    QCOMPARE(restored.smtpPort, 465);
    QCOMPARE(restored.caFile, a.caFile);
    QVERIFY(restored.imapStartTls);
    QVERIFY(restored.password.isEmpty());
    QCOMPARE(accountFromMap({{"password", a.password}}).password, a.password);
    Draft d;
    d.id = QStringLiteral("draft-1");
    d.to = QStringLiteral("recipient@example.invalid");
    d.cc = QStringLiteral("copy@example.invalid");
    d.bcc = QStringLiteral("blind@example.invalid");
    d.subject = QStringLiteral("Unicode 合成邮件");
    d.body = QStringLiteral("Line 1\nLine 2");
    d.inReplyTo = QStringLiteral("<test@example.invalid>");
    d.references = QStringLiteral("<parent@example.invalid>");
    d.updated = QDateTime::fromString(QStringLiteral("2026-10-29T12:34:56.789Z"), Qt::ISODateWithMs);
    d.state = QStringLiteral("unknown");
    d.attachments.append({QStringLiteral("test.txt"), QStringLiteral("text/plain"),
                          QStringLiteral("/synthetic/test.txt"), 42, QByteArray("not-ui-data")});
    const auto m = draftMap(d);
    const auto copy = draftFromMap(m);
    QCOMPARE(copy.id, d.id);
    QCOMPARE(copy.to, d.to);
    QCOMPARE(copy.cc, d.cc);
    QCOMPARE(copy.bcc, d.bcc);
    QCOMPARE(copy.subject, d.subject);
    QCOMPARE(copy.body, d.body);
    QCOMPARE(copy.inReplyTo, d.inReplyTo);
    QCOMPARE(copy.references, d.references);
    QCOMPARE(copy.updated, d.updated);
    QCOMPARE(copy.state, d.state);
    QCOMPARE(copy.attachments.size(), 1);
    QCOMPARE(copy.attachments.first().path, d.attachments.first().path);
    QCOMPARE(copy.attachments.first().name, d.attachments.first().name);
    QCOMPARE(copy.attachments.first().mimeType, d.attachments.first().mimeType);
    QCOMPARE(copy.attachments.first().size, 42);
    QVERIFY(copy.attachments.first().bytes.isEmpty());
    Content c;
    c.summary = message("1");
    c.text = QStringLiteral("safe plain text");
    c.attachments = d.attachments;
    const auto cm = contentMap(c);
    QCOMPARE(cm.value("uid").toString(), QStringLiteral("1"));
    QCOMPARE(cm.value("summary").toMap(), summaryMap(c.summary));
    QVERIFY(!cm.value("attachments").toList().first().toMap().contains("bytes"));
}
void CoreTests::accountAndFolderPersistence()
{
    QTemporaryDir dir(temporaryTemplate());
    QVERIFY(dir.isValid());
    Account a;
    a.address = QStringLiteral("test@example.invalid");
    a.username = a.address;
    a.imapHost = QStringLiteral("imap.example.invalid");
    a.smtpHost = QStringLiteral("smtp.example.invalid");
    a.password = QStringLiteral("SYNTHETIC-SECRET-MUST-NOT-REACH-SQLITE");
    const QList<Folder> folders = {{"INBOX", "Inbox", 5}, {"Sent", "Sent mail", 2}};
    {
        MailDatabase db;
        QVERIFY2(db.open(dir.path()), qPrintable(db.error()));
        QVERIFY(db.saveAccount(a));
        QVERIFY(db.saveFolders(folders));
        QVERIFY(!db.saveFolders({{"duplicate", "a", 0}, {"duplicate", "b", 0}}));
        QCOMPARE(db.folders().size(), 2); // rollback preserves the previous list
    }
    {
        MailDatabase db;
        QVERIFY(db.open(dir.path()));
        const auto restored = db.account();
        QVERIFY(restored.password.isEmpty());
        QCOMPARE(restored.address, a.address);
        QCOMPARE(restored.imapHost, a.imapHost);
        QCOMPARE(restored.smtpHost, a.smtpHost);
        const auto restoredFolders = db.folders();
        QCOMPARE(restoredFolders.size(), folders.size());
        for (int i = 0; i < folders.size(); ++i) {
            QCOMPARE(restoredFolders[i].id, folders[i].id);
            QCOMPARE(restoredFolders[i].title, folders[i].title);
            QCOMPARE(restoredFolders[i].total, folders[i].total);
        }
    }
    for (const QString &filename : QDir(dir.path()).entryList(QDir::Files)) {
        QFile f(dir.filePath(filename));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!f.readAll().contains(a.password.toUtf8()));
    }
    const auto permissions = QFile::permissions(dir.filePath("mail.sqlite"));
    QVERIFY(!(permissions & (QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther)));
}
void CoreTests::accountChangeClearsMailboxCache()
{
    QTemporaryDir dir(temporaryTemplate());
    MailDatabase db;
    QVERIFY(db.open(dir.path()));
    Account a;
    a.address = a.username = QStringLiteral("first@example.invalid");
    a.imapHost = QStringLiteral("imap.example.invalid");
    QVERIFY(db.saveAccount(a));
    QVERIFY(db.saveFolders({{"INBOX", "Inbox", 1}}));
    QVERIFY(db.savePage(page({message("1")})));
    Content c;
    c.summary = message("1");
    c.text = QStringLiteral("first mailbox content");
    QVERIFY(db.saveContent(c));
    Draft d;
    d.id = QStringLiteral("local-unsent");
    QVERIFY(db.saveDraft(d));
    a.password = QStringLiteral("session-secret");
    QVERIFY(db.saveAccount(a));
    QCOMPARE(db.cachedPage("INBOX").messages.size(), 1);
    a.address = a.username = QStringLiteral("second@example.invalid");
    QVERIFY(db.saveAccount(a));
    QVERIFY(db.folders().isEmpty());
    QVERIFY(db.cachedPage("INBOX").messages.isEmpty());
    QVERIFY(!db.content("INBOX", "100", "1"));
    QCOMPARE(db.drafts().size(), 1);
}
void CoreTests::draftRestartAndStates()
{
    QTemporaryDir dir(temporaryTemplate());
    QVERIFY(dir.isValid());
    const QString attachmentPath = dir.filePath("synthetic-attachment.txt");
    QFile attachment(attachmentPath);
    QVERIFY(attachment.open(QIODevice::WriteOnly));
    QCOMPARE(attachment.write("synthetic fixture"), 17);
    attachment.close();
    Draft draft;
    draft.id = QStringLiteral("durable-draft");
    draft.to = QStringLiteral("nobody@example.invalid");
    draft.subject = QStringLiteral("Unsent local fixture");
    draft.body = QStringLiteral("This must survive restart");
    draft.attachments.append({"synthetic-attachment.txt", "text/plain", attachmentPath, 17, {}});
    {
        MailDatabase db;
        QVERIFY(db.open(dir.path()));
        QVERIFY(db.saveDraft(draft));
        QVERIFY(db.markDraftState(draft.id, QStringLiteral("sending")));
    }
    {
        MailDatabase db;
        QVERIFY(db.open(dir.path()));
        const auto drafts = db.drafts();
        QCOMPARE(drafts.size(), 1);
        const auto &restored = drafts.first();
        QCOMPARE(restored.id, draft.id);
        QCOMPARE(restored.body, draft.body);
        QCOMPARE(restored.state, QStringLiteral("unknown")); // interrupted sending is never auto-retried
        QVERIFY(restored.updated.isValid());
        QCOMPARE(restored.attachments.size(), 1);
        QCOMPARE(restored.attachments.first().path, attachmentPath);
        QCOMPARE(restored.attachments.first().size, 17);
        QCOMPARE(restored.attachments.first().mimeType, QStringLiteral("text/plain"));
        QVERIFY(db.markDraftState(draft.id, QStringLiteral("unknown")));
        QVERIFY(!db.markDraftState(QStringLiteral("missing"), QStringLiteral("accepted")));
    }
    {
        MailDatabase db;
        QVERIFY(db.open(dir.path()));
        QCOMPARE(db.drafts().first().state, QStringLiteral("unknown"));
        QVERIFY(db.deleteDraft(draft.id));
        QVERIFY(db.drafts().isEmpty());
    }
    MailDatabase db;
    QVERIFY(db.open(dir.path()));
    QVERIFY(db.drafts().isEmpty());
    QVERIFY(QFile::exists(attachmentPath)); // deleting metadata does not destroy user files
}
void CoreTests::authoritativeDraftSendStates()
{
    QTemporaryDir dir(temporaryTemplate());
    MailDatabase db;
    QVERIFY(db.open(dir.path()));
    Draft stale;
    stale.id = QStringLiteral("send-guard");
    stale.to = QStringLiteral("nobody@example.invalid");
    QVERIFY(db.saveDraft(stale));
    QVERIFY(db.markDraftState(stale.id, "sending"));
    QVERIFY(!db.markDraftState(stale.id, "sending"));
    QVERIFY(!db.markDraftState(stale.id, "draft"));
    stale.body = QStringLiteral("edited while asynchronous send is in flight");
    QVERIFY(db.saveDraft(stale));
    QCOMPARE(db.drafts().first().state, QStringLiteral("sending"));
    QCOMPARE(db.drafts().first().body, stale.body);
    stale.state = QStringLiteral("rejected");
    QVERIFY(db.saveDraft(stale));
    QCOMPARE(db.drafts().first().state, QStringLiteral("sending"));
    QVERIFY(db.markDraftState(stale.id, "rejected"));
    QVERIFY(db.markDraftState(stale.id, "sending"));
    QVERIFY(db.markDraftState(stale.id, "unknown"));
    QVERIFY(!db.markDraftState(stale.id, "sending"));
    QVERIFY(!db.markDraftState(stale.id, "draft"));
    QVERIFY(!db.markDraftState(stale.id, "rejected"));
    QVERIFY(!db.markDraftState(stale.id, "accepted"));
    stale.state = QStringLiteral("draft");
    QVERIFY(db.saveDraft(stale));
    QCOMPARE(db.drafts().first().state, QStringLiteral("unknown"));
    QVERIFY(db.markDraftState(stale.id, "unknown"));
    Draft accepted;
    accepted.id = QStringLiteral("accepted-guard");
    QVERIFY(db.saveDraft(accepted));
    QVERIFY(db.markDraftState(accepted.id, "sending"));
    QVERIFY(db.markDraftState(accepted.id, "accepted"));
    QVERIFY(db.saveDraft(accepted));
    QVERIFY(!db.markDraftState(accepted.id, "sending"));
    QVERIFY(!db.markDraftState(accepted.id, "rejected"));
    QVERIFY(!db.markDraftState(accepted.id, "unknown"));
    QVERIFY(db.markDraftState(accepted.id, "accepted"));
    for (const auto &d : db.drafts())
        QCOMPARE(d.state, d.id == accepted.id ? QStringLiteral("accepted") : QStringLiteral("unknown"));
    db.close();
    QVERIFY(db.open(dir.path()));
    for (const auto &d : db.drafts())
        QCOMPARE(d.state, d.id == accepted.id ? QStringLiteral("accepted") : QStringLiteral("unknown"));
}
void CoreTests::uidValidityIsolation()
{
    QTemporaryDir dir(temporaryTemplate());
    QVERIFY(dir.isValid());
    Content old;
    old.summary = message("7");
    old.text = QStringLiteral("old-generation-body");
    old.attachments.append({"binary.bin", "application/octet-stream", {}, 4, QByteArray("\x00\x01\xff\x00", 4)});
    {
        MailDatabase db;
        QVERIFY(db.open(dir.path()));
        QVERIFY(db.savePage(page({old.summary})));
        QVERIFY(db.saveContent(old));
        const auto cached = db.content("INBOX", "100", "7");
        QVERIFY(cached.has_value());
        QCOMPARE(cached->attachments.first().bytes, old.attachments.first().bytes);
        QVERIFY(!db.content("Other", "100", "7"));
        QVERIFY(!db.content("INBOX", "101", "7"));
        QVERIFY(db.invalidateUidValidity("INBOX", "101"));
        QVERIFY(!db.content("INBOX", "100", "7"));
        QVERIFY(!db.content("INBOX", "101", "7"));
        QVERIFY(db.cachedPage("INBOX").messages.isEmpty());
        QVERIFY(!db.saveContent(old));
        QVERIFY(!db.savePage(page({old.summary}))); // stale in-flight page cannot revive old generation
        QVERIFY(!db.invalidateUidValidity("INBOX", "100"));
        Content fresh;
        fresh.summary = message("7", "101");
        fresh.text = QStringLiteral("new-generation-body");
        QVERIFY(db.savePage(page({fresh.summary}, "101")));
        QVERIFY(db.saveContent(fresh));
        QCOMPARE(db.content("INBOX", "101", "7")->text, fresh.text);
        Page bad = page({message("8", "102")}, "103");
        QVERIFY(!db.savePage(bad));
        QCOMPARE(db.cachedPage("INBOX").uidValidity, QStringLiteral("101"));
        QVERIFY(db.content("INBOX", "101", "7"));
    }
    MailDatabase db;
    QVERIFY(db.open(dir.path()));
    QVERIFY(!db.content("INBOX", "100", "7"));
    QVERIFY(!db.savePage(page({old.summary})));
    QCOMPARE(db.content("INBOX", "101", "7")->text, QStringLiteral("new-generation-body"));
}
void CoreTests::paginationAndSearch()
{
    QTemporaryDir dir(temporaryTemplate());
    MailDatabase db;
    QVERIFY(db.open(dir.path()));
    QList<Summary> messages;
    for (int i = 1; i <= 12; ++i) {
        auto s = message(QString::number(i));
        s.subject = i % 2 ? QStringLiteral("Odd 合成") : QStringLiteral("Even 100%_literal");
        messages.append(s);
    }
    QVERIFY(db.savePage(page(messages)));
    const auto first = db.cachedPage("INBOX", 0, 5);
    QCOMPARE(first.messages.size(), 5);
    QCOMPARE(first.messages.first().uid, QStringLiteral("12"));
    QCOMPARE(first.messages.last().uid, QStringLiteral("8"));
    QVERIFY(first.hasMore);
    QCOMPARE(first.nextOffset, 5);
    const auto second = db.cachedPage("INBOX", 5, 5);
    QCOMPARE(second.messages.first().uid, QStringLiteral("7"));
    const auto third = db.cachedPage("INBOX", 10, 5);
    QCOMPARE(third.messages.size(), 2);
    QVERIFY(!third.hasMore);
    QCOMPARE(third.nextOffset, 12);
    const auto search = db.cachedPage("INBOX", 0, 2, "EVEN");
    QCOMPARE(search.messages.size(), 2);
    QCOMPARE(search.messages[0].uid, QStringLiteral("12"));
    QCOMPARE(search.messages[1].uid, QStringLiteral("10"));
    QVERIFY(search.hasMore);
    QCOMPARE(db.cachedPage("INBOX", 4, 2, "EVEN").messages.first().uid, QStringLiteral("4"));
    QCOMPARE(db.cachedPage("INBOX", 0, 20, "%_").messages.size(), 6); // not SQL wildcard syntax
    QCOMPARE(db.cachedPage("INBOX", 0, 20, "合成").messages.size(), 6);
    QVERIFY(db.cachedPage("INBOX", 0, 20, "' OR 1=1 --").messages.isEmpty());
    messages[0].preview = QStringLiteral("updated preview");
    QVERIFY(db.savePage(page({messages[0]})));
    QCOMPARE(db.cachedPage("INBOX", 0, 20).messages.size(), 12);
    QCOMPARE(db.cachedPage("INBOX", 0, 20, "updated").messages.size(), 1);
    QVERIFY(db.cachedPage("INBOX", -1, 20).messages.isEmpty());
    QVERIFY(!db.error().isEmpty());
}
void CoreTests::unreadPaginationAndSearch()
{
    QTemporaryDir dir(temporaryTemplate());
    MailDatabase db;
    QVERIFY(db.open(dir.path()));
    QList<Summary> summaries;
    for (int uid = 1; uid <= 200; ++uid) {
        auto s = message(QString::number(uid));
        // Seventy newest messages are read: filtering only a raw first page
        // would incorrectly report an empty mailbox, not 130 unread messages.
        s.unread = uid <= 130;
        s.subject = uid % 2 ? QStringLiteral("Odd 合成") : QStringLiteral("Even 100%_literal");
        if (!s.unread) s.subject += QStringLiteral(" read-only-token");
        summaries.append(s);
    }
    QVERIFY(db.savePage(page(summaries)));
    QList<Summary> unread;
    int offset = 0;
    for (int batch = 0; batch < 3; ++batch) {
        const auto p = db.cachedPage("INBOX", offset, 50, {}, true);
        QCOMPARE(p.uidValidity, QStringLiteral("100"));
        QCOMPARE(p.messages.size(), batch == 2 ? 30 : 50);
        QCOMPARE(p.messages.first().uid, QString::number(130 - offset));
        QCOMPARE(p.hasMore, batch < 2);
        QCOMPARE(p.nextOffset, offset + p.messages.size());
        unread.append(p.messages);
        offset = p.nextOffset;
    }
    QCOMPARE(offset, 130);
    for (int row = 0; row < unread.size(); ++row) {
        QVERIFY(unread[row].unread);
        QCOMPARE(unread[row].uid, QString::number(130 - row));
    }
    const auto combined = db.cachedPage("INBOX", 0, 50, "EVEN", true);
    QCOMPARE(combined.messages.size(), 50);
    QCOMPARE(combined.messages.first().uid, QStringLiteral("130"));
    QCOMPARE(combined.messages.last().uid, QStringLiteral("32"));
    QVERIFY(combined.hasMore);
    QCOMPARE(combined.nextOffset, 50);
    const auto combinedTail = db.cachedPage("INBOX", combined.nextOffset, 50, "EVEN", true);
    QCOMPARE(combinedTail.messages.size(), 15);
    QCOMPARE(combinedTail.messages.first().uid, QStringLiteral("30"));
    QCOMPARE(combinedTail.messages.last().uid, QStringLiteral("2"));
    QCOMPARE(combinedTail.nextOffset, 65);
    QVERIFY(!combinedTail.hasMore);
    QCOMPARE(db.cachedPage("INBOX", 0, 200, "%_", true).messages.size(), 65);
    QCOMPARE(db.cachedPage("INBOX", 0, 200, "合成", true).messages.size(), 65);
    QCOMPARE(db.cachedPage("INBOX", 0, 200, "read-only-token").messages.size(), 70);
    for (const auto &query : {QStringLiteral("missing"), QStringLiteral("read-only-token"), QStringLiteral("' OR 1=1 --")}) {
        const auto empty = db.cachedPage("INBOX", 0, 50, query, true);
        QVERIFY(empty.messages.isEmpty());
        QVERIFY(!empty.hasMore);
        QCOMPARE(empty.nextOffset, 0);
        QCOMPARE(empty.uidValidity, QStringLiteral("100"));
    }
    const auto exhausted = db.cachedPage("INBOX", 150, 50, {}, true);
    QVERIFY(exhausted.messages.isEmpty());
    QVERIFY(!exhausted.hasMore);
    QCOMPARE(exhausted.nextOffset, 150);
    const auto raw = db.cachedPage("INBOX", 0, 200);
    QCOMPARE(raw.messages.size(), 200);
    QVERIFY(!raw.messages.first().unread); // Reads did not mutate cached flags.
    QCOMPARE(raw.messages.first().uid, QStringLiteral("200"));
    QVERIFY(db.cachedPage("INBOX", -1, 50, {}, true).messages.isEmpty());
    QVERIFY(!db.error().isEmpty());
}
void CoreTests::unreadUidValidityIsolation()
{
    QTemporaryDir dir(temporaryTemplate());
    MailDatabase db;
    QVERIFY(db.open(dir.path()));
    auto oldRead = message("1");
    oldRead.unread = false;
    auto oldUnread = message("2");
    oldUnread.unread = true;
    QVERIFY(db.savePage(page({oldRead, oldUnread}, "100")));
    QCOMPARE(db.cachedPage("INBOX", 0, 20, {}, true).messages.size(), 1);
    QVERIFY(db.invalidateUidValidity("INBOX", "101"));
    QVERIFY(db.cachedPage("INBOX", 0, 20, {}, true).messages.isEmpty());
    auto current = message("3", "101");
    current.unread = true;
    QVERIFY(db.savePage(page({current}, "101")));
    QCOMPARE(db.cachedPage("INBOX", 0, 20, {}, true).messages.first().uid, QStringLiteral("3"));
    QVERIFY(!db.savePage(page({oldUnread}, "100")));
    QCOMPARE(db.cachedPage("INBOX", 0, 20, {}, true).messages.first().uid, QStringLiteral("3"));
}
void CoreTests::migrations()
{
    QTemporaryDir dir(temporaryTemplate());
    QVERIFY(dir.isValid());
    const QString connection = QStringLiteral("migration-fixture");
    Draft old;
    old.id = QStringLiteral("migrated-draft");
    old.state = QStringLiteral("unknown");
    old.updated = QDateTime::fromString(QStringLiteral("2026-01-01T01:02:03.004Z"), Qt::ISODateWithMs);
    old.attachments.append({"retained.txt", "text/plain", "/synthetic/retained.txt", 9, {}});
    {
        auto sql = QSqlDatabase::addDatabase("QSQLITE", connection);
        sql.setDatabaseName(dir.filePath("mail.sqlite"));
        QVERIFY(sql.open());
        QSqlQuery q(sql);
        const QStringList schema = {
            "CREATE TABLE accounts (slot INTEGER PRIMARY KEY CHECK(slot=1),payload TEXT NOT NULL)",
            "CREATE TABLE folders (id TEXT PRIMARY KEY,title TEXT NOT NULL,total INTEGER NOT NULL,position INTEGER NOT NULL)",
            "CREATE TABLE folder_state (folder TEXT PRIMARY KEY,uid_validity TEXT NOT NULL)",
            "CREATE TABLE summaries (folder TEXT NOT NULL,uid_validity TEXT NOT NULL,uid TEXT NOT NULL,payload TEXT NOT NULL,PRIMARY KEY(folder,uid_validity,uid))",
            "CREATE TABLE contents (folder TEXT NOT NULL,uid_validity TEXT NOT NULL,uid TEXT NOT NULL,payload TEXT NOT NULL,PRIMARY KEY(folder,uid_validity,uid))",
            "CREATE TABLE drafts (id TEXT PRIMARY KEY,payload TEXT NOT NULL)",
            "PRAGMA user_version=1"
        };
        for (const auto &statement : schema)
            QVERIFY2(q.exec(statement), qPrintable(q.lastError().text()));
        q.prepare("INSERT INTO drafts(id,payload) VALUES(?,?)");
        q.addBindValue(old.id);
        q.addBindValue(QString::fromUtf8(QJsonDocument::fromVariant(draftMap(old)).toJson()));
        QVERIFY(q.exec());
    }
    QSqlDatabase::removeDatabase(connection);
    {
        MailDatabase db;
        QVERIFY2(db.open(dir.path()), qPrintable(db.error()));
        const auto drafts = db.drafts();
        QCOMPARE(drafts.size(), 1);
        QCOMPARE(drafts.first().id, old.id);
        QCOMPARE(drafts.first().state, old.state);
        QCOMPARE(drafts.first().updated, old.updated);
        QCOMPARE(drafts.first().attachments.first().path, old.attachments.first().path);
    }
    MailDatabase reopened;
    QVERIFY(reopened.open(dir.path()));
    QCOMPARE(reopened.drafts().first().state, old.state);
}
void CoreTests::futureSchemaRejected()
{
    QTemporaryDir dir(temporaryTemplate());
    QVERIFY(dir.isValid());
    const QString connection = QStringLiteral("future-fixture");
    {
        auto sql = QSqlDatabase::addDatabase("QSQLITE", connection);
        sql.setDatabaseName(dir.filePath("mail.sqlite"));
        QVERIFY(sql.open());
        QSqlQuery q(sql);
        QVERIFY(q.exec("PRAGMA user_version=999"));
    }
    QSqlDatabase::removeDatabase(connection);
    MailDatabase db;
    QVERIFY(!db.open(dir.path()));
    QVERIFY(db.error().contains("999"));
    QVERIFY(!db.isOpen());
}
void CoreTests::independentConnectionsAndErrors()
{
    QTemporaryDir a(temporaryTemplate()), b(temporaryTemplate());
    MailDatabase one, two;
    QVERIFY(!one.open({}));
    QVERIFY(!one.saveDraft({}));
    QVERIFY(one.open(a.path()));
    QVERIFY(two.open(b.path()));
    QVERIFY(one.savePage(page({message("1")})));
    QVERIFY(two.cachedPage("INBOX").messages.isEmpty());
    MailDatabase sameRoot;
    QVERIFY(sameRoot.open(a.path()));
    QCOMPARE(sameRoot.cachedPage("INBOX").messages.size(), 1);
    sameRoot.close();
    QVERIFY(one.isOpen());
    auto *worker = QThread::create([&one] { QVERIFY(!one.folders().size()); });
    worker->start();
    QVERIFY(worker->wait());
    delete worker;
    QVERIFY(one.error().contains("owning thread"));
    QCOMPARE(one.cachedPage("INBOX").messages.size(), 1);
}
void CoreTests::messageModelIdentity()
{
    MessageListModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QItemSelectionModel selection(&model);
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
    const auto a = message("1"), b = message("2"), c = message("3");
    model.replace({a, b, c});
    QCOMPARE(model.count(), 3);
    selection.setCurrentIndex(model.index(1), QItemSelectionModel::ClearAndSelect);
    QPersistentModelIndex persistent(model.index(1));
    auto updated = b;
    updated.subject = QStringLiteral("updated");
    model.replace({c, updated, a});
    QVERIFY(persistent.isValid());
    QCOMPARE(persistent.data(MessageListModel::UidRole).toString(), b.uid);
    QCOMPARE(selection.currentIndex().data(MessageListModel::UidRole).toString(), b.uid);
    QCOMPARE(selection.selectedIndexes().size(), 1);
    QCOMPARE(selection.selectedIndexes().first().data(MessageListModel::UidRole).toString(), b.uid);
    QCOMPARE(persistent.data(MessageListModel::SubjectRole).toString(), updated.subject);
    QCOMPARE(resets.count(), 0);
    QCOMPARE(changes.count(), 1);
    model.append({updated, message("4"), message("4")});
    QCOMPARE(model.count(), 4);
    model.replace({message("4"), updated});
    QCOMPARE(persistent.row(), 1);
    model.replace({updated, message("4")});
    QCOMPARE(persistent.row(), 0);
    QCOMPARE(model.get(0).value("uid").toString(), b.uid);
    QCOMPARE(model.indexOf("INBOX", "100", "2"), 0);
    QVERIFY(model.get(-1).isEmpty());
    QVERIFY(!model.data(model.index(0), Qt::UserRole - 1).isValid());
    QCOMPARE(model.roleNames().size(), 11);
    model.replace({message("2", "101")});
    QVERIFY(!persistent.isValid());
    QCOMPARE(model.indexOf("INBOX", "100", "2"), -1);
    QVERIFY(!model.update(a));
    model.clear();
    QCOMPARE(model.count(), 0);
}
void CoreTests::folderModelIdentity()
{
    FolderListModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    model.replace({{"INBOX", "Inbox", 5}, {"Sent", "Sent", 2}});
    QPersistentModelIndex sent(model.index(1));
    model.replace({{"Sent", "Sent mail", 3}, {"INBOX", "Inbox", 5}, {"Drafts", "Drafts", 1}});
    QVERIFY(sent.isValid());
    QCOMPARE(sent.row(), 0);
    QCOMPARE(sent.data(FolderListModel::IdRole).toString(), QStringLiteral("Sent"));
    QCOMPARE(sent.data(FolderListModel::TitleRole).toString(), QStringLiteral("Sent mail"));
    QCOMPARE(sent.data(FolderListModel::TotalRole).toInt(), 3);
    QCOMPARE(reset.count(), 0);
    model.append({{"INBOX", "Inbox", 6}, {"Archive", "Archive", 1}, {"Archive", "Archive", 2}});
    QCOMPARE(model.count(), 4);
    QCOMPARE(model.get(model.indexOf("Archive")).value("total").toInt(), 2);
    model.replace({{"INBOX", "Inbox", 6}});
    QVERIFY(!sent.isValid());
    QVERIFY(!model.update({"missing", "", 0}));
    QVERIFY(model.get(50).isEmpty());
    QCOMPARE(model.roleNames().size(), 3);
    model.clear();
    QCOMPARE(model.count(), 0);
}
void CoreTests::modelMutationStress()
{
    MessageListModel messages;
    FolderListModel folders;
    QAbstractItemModelTester messageTester(&messages, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QAbstractItemModelTester folderTester(&folders, QAbstractItemModelTester::FailureReportingMode::QtTest);
    // Deterministic mutation sweep exercises inserts, backwards moves, removals and duplicates.
    for (int iteration = 0; iteration < 80; ++iteration) {
        QList<Summary> summaries;
        QList<Folder> folderValues;
        for (int i = 0; i < 12; ++i) {
            const int id = (i * 7 + iteration * 3) % 19;
            if ((id + iteration) % 3 == 0)
                continue;
            summaries.append(message(QString::number(id)));
            folderValues.append({QString::number(id), QString::number(iteration), id});
        }
        messages.replace(summaries);
        folders.replace(folderValues);
        QCOMPARE(messages.count(), summaries.size());
        QCOMPARE(folders.count(), folderValues.size());
        for (int row = 0; row < summaries.size(); ++row) {
            QCOMPARE(messages.get(row).value("uid").toString(), summaries[row].uid);
            QCOMPARE(folders.get(row).value("id").toString(), folderValues[row].id);
        }
    }
}
void CoreTests::credentialConstructionDoesNotAccessBackend()
{
    CredentialStore store(QStringLiteral("org.nextkde.NkMail.synthetic-test-never-started"));
    QSignalSpy ready(&store, &CredentialStore::passwordReady);
    QSignalSpy error(&store, &CredentialStore::error);
    QCoreApplication::processEvents();
    QCOMPARE(ready.count(), 0);
    QCOMPARE(error.count(), 0);
    QVERIFY(store.children().isEmpty());
    // Deliberately do not invoke read/write/remove: tests never contact the machine keyring.
}
QTEST_GUILESS_MAIN(CoreTests)
#include "tst_core.moc"
