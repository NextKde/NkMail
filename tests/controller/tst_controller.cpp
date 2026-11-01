#include "MailController.h"
#include "MessageListModel.h"
#include "mail/backend.h"
#include "storage/CredentialStore.h"
#include "storage/MailDatabase.h"
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QSemaphore>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtTest>
#include <atomic>
#include <functional>
#include <stdexcept>

using namespace nkmail;
namespace synthetic {
std::function<QList<Folder>(const Account &)> folders;
std::function<Page(const Account &, const QString &, int, int)> page;
std::function<Content(const Account &, const QString &, const QString &, const QString &)> content;
std::function<SendResult(const Account &, const Draft &)> send;
std::function<void(CredentialStore *, const QString &)> readCredential;
QMutex callsMutex;
QList<int> offsets;
QStringList contentUids;
std::atomic_int folderCalls{0}, pageCalls{0}, contentCalls{0}, sendCalls{0};
std::atomic_int credentialReads{0}, credentialWrites{0}, credentialDeletes{0};
QString lastPassword;
QStringList credentialReadKeys, credentialWriteKeys, credentialDeleteKeys;
void reset()
{
    folders = [](const Account &) { return QList<Folder>{{"INBOX", "Inbox", 120}, {"Archive", "Archive", 0}}; };
    page = [](const Account &, const QString &folder, int offset, int limit) {
        Page p;
        p.folder = folder;
        p.uidValidity = QStringLiteral("100");
        for (int i = offset; i < qMin(offset + limit, 120); ++i) {
            Summary s;
            s.folder = folder; s.uidValidity = p.uidValidity; s.uid = QString::number(120 - i);
            s.subject = i % 2 ? QStringLiteral("odd") : QStringLiteral("even");
            s.senderAddress = QStringLiteral("sender@example.invalid");
            p.messages.append(s);
        }
        p.nextOffset = offset + int(p.messages.size());
        p.hasMore = p.nextOffset < 120;
        return p;
    };
    content = [](const Account &, const QString &folder, const QString &validity, const QString &uid) {
        Content c;
        c.summary.folder = folder; c.summary.uidValidity = validity; c.summary.uid = uid;
        c.text = QStringLiteral("Synthetic body ") + uid;
        c.attachments.append({"fixture.txt", "text/plain", {}, 9, QByteArray("synthetic")});
        return c;
    };
    send = [](const Account &, const Draft &) { return SendResult{SendResult::Accepted, {}}; };
    readCredential = [](CredentialStore *store, const QString &id) {
        QTimer::singleShot(0, store, [store, id] { emit store->passwordReady(id, QStringLiteral("synthetic-password")); });
    };
    offsets.clear(); contentUids.clear(); lastPassword.clear();
    credentialReadKeys.clear(); credentialWriteKeys.clear(); credentialDeleteKeys.clear();
    folderCalls = 0; pageCalls = 0; contentCalls = 0; sendCalls = 0;
    credentialReads = 0; credentialWrites = 0; credentialDeletes = 0;
}
}
// Link-time synthetic boundaries; production backend/keychain translation units
// are deliberately not linked. All remaining code is the actual application.
namespace nkmail {
QList<Folder> MailioBackend::folders(const Account &a)
{
    ++synthetic::folderCalls;
    return synthetic::folders(a);
}
Page MailioBackend::page(const Account &a, const QString &folder, int offset, int limit)
{
    ++synthetic::pageCalls;
    { QMutexLocker lock(&synthetic::callsMutex); synthetic::offsets.append(offset); }
    return synthetic::page(a, folder, offset, limit);
}
Content MailioBackend::content(const Account &a, const QString &folder, const QString &validity, const QString &uid)
{
    ++synthetic::contentCalls;
    { QMutexLocker lock(&synthetic::callsMutex); synthetic::contentUids.append(uid); }
    return synthetic::content(a, folder, validity, uid);
}
SendResult MailioBackend::send(const Account &a, const Draft &d, QByteArray *)
{
    ++synthetic::sendCalls;
    return synthetic::send(a, d);
}
QString MailioBackend::sentFolder(const Account &)
{
    return QStringLiteral("Sent");
}
void MailioBackend::appendSent(const Account &, const QString &, const QByteArray &)
{
}
QList<QVariantMap> MailioBackend::search(const Account &, const QString &, const QString &, const QString &, int)
{
    return {};
}
void MailioBackend::storeFlags(const Account &, const QString &, const QString &, const QString &, const QStringList &, bool)
{
}
void MailioBackend::copyTo(const Account &, const QString &, const QString &, const QString &, const QString &)
{
}
void MailioBackend::expunge(const Account &, const QString &, const QString &, const QString &, bool *)
{
}
void MailioBackend::storeFlagsMany(const Account &, const QString &, const QString &, const QStringList &, const QStringList &, bool)
{
}
void MailioBackend::copyMany(const Account &, const QString &, const QString &, const QStringList &, const QString &)
{
}
void MailioBackend::expungeMany(const Account &, const QString &, const QString &, const QStringList &, bool *)
{
}
CredentialStore::CredentialStore(QObject *parent) : CredentialStore(QStringLiteral("synthetic"), parent) {}
CredentialStore::CredentialStore(const QString &service, QObject *parent) : QObject(parent), m_service(service) {}
void CredentialStore::read(const QString &id)
{
    ++synthetic::credentialReads;
    synthetic::credentialReadKeys.append(id);
    synthetic::readCredential(this, id);
}
void CredentialStore::write(const QString &id, const QString &password)
{
    ++synthetic::credentialWrites;
    synthetic::credentialWriteKeys.append(id);
    synthetic::lastPassword = password;
    QTimer::singleShot(0, this, [this, id] { emit passwordSaved(id); });
}
void CredentialStore::remove(const QString &id)
{
    ++synthetic::credentialDeletes;
    synthetic::credentialDeleteKeys.append(id);
    QTimer::singleShot(0, this, [this, id] { emit passwordDeleted(id); });
}
}
namespace {
QString temporaryTemplate() { return QStringLiteral(NKMAIL_CONTROLLER_TMP_ROOT "/case-XXXXXX"); }
Account account()
{
    Account a;
    a.address = a.username = QStringLiteral("test@example.invalid");
    a.imapHost = QStringLiteral("imap.example.invalid");
    a.smtpHost = QStringLiteral("smtp.example.invalid");
    return a;
}
bool seed(const QString &root, int cachedCount = 0, bool restore = true)
{
    MailDatabase db;
    const QList<Folder> folders{{"INBOX", "Inbox", 120}, {"Archive", "Archive", 0}};
    if (!db.open(root) || !db.saveAccount(account()) || !db.saveFolders(folders))
        return false;
    if (cachedCount) {
        auto p = synthetic::page(account(), "INBOX", 0, cachedCount);
        if (!db.savePage(p))
            return false;
    }
    QSettings settings(QDir(root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue("restoreAllowed", restore);
    settings.sync();
    return settings.status() == QSettings::NoError;
}
MessageListModel *messages(MailController &controller)
{
    return qobject_cast<MessageListModel *>(controller.messages());
}
QList<int> offsets()
{
    QMutexLocker lock(&synthetic::callsMutex);
    return synthetic::offsets;
}
// Release blocked synthetic workers on every assertion exit, before controller
// destruction waits for the dedicated pool.
struct ReleaseOnExit {
    QSemaphore &semaphore;
    ~ReleaseOnExit() { semaphore.release(20); }
};
}
class ControllerTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QVERIFY(QDir().mkpath(QStringLiteral(NKMAIL_CONTROLLER_TMP_ROOT))); }
    void init() { synthetic::reset(); }
    void demoNeverUsesExternalBoundaries();
    void refreshAndPagination();
    void cachedSearchNeverFetches();
    void refreshFailureStillPaginatesCache();
    void failedNetworkAppendUsesCachedTail();
    void latePageIgnoredDuringCachedSearch();
    void newerSelectionWinsOverLateBody();
    void draftAttachmentsSurviveControllerRestart();
    void uidValidityChangeRestartsPagination();
    void staleBodySelectionCannotExportPreviousAttachment();
    void lateResultsIgnoredAfterFolderChange();
    void lateCredentialCannotUndoDisconnect();
    void credentialKeysAreProfileScoped();
    void lateCredentialCompletionPreservesDisconnectTombstone();
    void accountTransportChangeClearsVisibleCache();
    void staleDraftMapNeverResends();
    void interruptedSendRestoresUnknown();
    void composeReplyAndForward();
    void replyAllQuotedNamesAndExactSelfExclusion();
    void unsupportedReplyRecipientsFailClosed();
    void attachmentExportAndDraftDiscard();
};
void ControllerTests::demoNeverUsesExternalBoundaries()
{
    QTemporaryDir root(temporaryTemplate());
    MailController controller(true, root.path());
    QTRY_COMPARE(messages(controller)->count(), 8);
    QVERIFY(!controller.selectedMessage().isEmpty());
    controller.refresh();
    controller.setSearch("design");
    QVERIFY(messages(controller)->count() > 0);
    auto draft = controller.newDraft();
    draft.insert("to", "nobody@example.invalid");
    controller.sendDraft(draft);
    QCOMPARE(controller.loadDraft(draft.value("id").toString()).value("state").toString(), QStringLiteral("draft"));
    QCOMPARE(synthetic::folderCalls.load(), 0);
    QCOMPARE(synthetic::pageCalls.load(), 0);
    QCOMPARE(synthetic::contentCalls.load(), 0);
    QCOMPARE(synthetic::sendCalls.load(), 0);
    QCOMPARE(synthetic::credentialReads.load(), 0);
    QCOMPARE(synthetic::credentialWrites.load(), 0);
    QCOMPARE(synthetic::credentialDeletes.load(), 0);
}
void ControllerTests::refreshAndPagination()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    MailController controller(false, root.path());
    QTRY_COMPARE(synthetic::credentialReads.load(), 1);
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.refresh();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(messages(controller)->count(), 50);
    QVERIFY(controller.hasMore());
    controller.loadMore();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(messages(controller)->count(), 100);
    controller.loadMore();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(messages(controller)->count(), 120);
    QVERIFY(!controller.hasMore());
    QCOMPARE(offsets(), QList<int>({0, 50, 100}));
    controller.loadMore();
    QCOMPARE(synthetic::pageCalls.load(), 3);
}
void ControllerTests::cachedSearchNeverFetches()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path(), 120));
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.setSearch("EVEN");
    QCOMPARE(messages(controller)->count(), 50);
    QVERIFY(controller.hasMore());
    controller.loadMore();
    QCOMPARE(messages(controller)->count(), 60);
    QVERIFY(!controller.hasMore());
    controller.refresh();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(synthetic::pageCalls.load(), 0);
    controller.setSearch("' OR 1=1 --");
    QCOMPARE(messages(controller)->count(), 0);
    QCOMPARE(synthetic::contentCalls.load(), 0);
}
void ControllerTests::refreshFailureStillPaginatesCache()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path(), 120));
    synthetic::folders = [](const Account &) -> QList<Folder> { throw BackendError("Synthetic offline failure"); };
    synthetic::page = [](const Account &, const QString &, int, int) -> Page { throw BackendError("Synthetic offline failure"); };
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    QCOMPARE(messages(controller)->count(), 50);
    controller.refresh();
    QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.error().isEmpty());
    controller.loadMore();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(messages(controller)->count(), 100);
    controller.loadMore();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(messages(controller)->count(), 120);
}
void ControllerTests::failedNetworkAppendUsesCachedTail()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path(), 120));
    const auto normal = synthetic::page;
    synthetic::page = [normal](const Account &a, const QString &folder, int offset, int limit) {
        if (offset > 0) throw BackendError("Synthetic network lost after first page");
        return normal(a, folder, offset, limit);
    };
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.refresh();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(messages(controller)->count(), 50);
    controller.loadMore();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(messages(controller)->count(), 100);
    QVERIFY(!controller.error().isEmpty());
    controller.loadMore();
    QCOMPARE(messages(controller)->count(), 120);
    QCOMPARE(offsets(), QList<int>({0, 50}));
}
void ControllerTests::latePageIgnoredDuringCachedSearch()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path(), 120));
    QSemaphore release;
    const auto normal = synthetic::page;
    synthetic::page = [&release, normal](const Account &a, const QString &folder, int offset, int limit) {
        release.acquire();
        return normal(a, folder, offset, limit);
    };
    MailController controller(false, root.path());
    ReleaseOnExit cleanup{release};
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.refresh();
    QTRY_COMPARE(synthetic::pageCalls.load(), 1);
    controller.setSearch("even");
    QCOMPARE(messages(controller)->count(), 50);
    release.release();
    // Drain the queued completion through another task in the same pool.
    controller.openMessage("120", "100");
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    QCOMPARE(controller.search(), QStringLiteral("even"));
    for (int row = 0; row < messages(controller)->count(); ++row)
        QCOMPARE(messages(controller)->get(row).value("subject").toString(), QStringLiteral("even"));
    controller.loadMore();
    QCOMPARE(messages(controller)->count(), 60);
    QCOMPARE(synthetic::pageCalls.load(), 1);
}
void ControllerTests::newerSelectionWinsOverLateBody()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    QSemaphore release;
    const auto normal = synthetic::content;
    synthetic::content = [&release, normal](const Account &a, const QString &folder, const QString &validity, const QString &uid) {
        if (uid == QLatin1String("120")) release.acquire();
        return normal(a, folder, validity, uid);
    };
    MailController controller(false, root.path());
    ReleaseOnExit cleanup{release};
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.openMessage("120", "100");
    QTRY_COMPARE(synthetic::contentCalls.load(), 1);
    controller.openMessage("119", "100");
    release.release();
    QTRY_COMPARE(controller.selectedMessage().value("uid").toString(), QStringLiteral("119"));
    QCOMPARE(controller.selectedUid(), QStringLiteral("119"));
    QCOMPARE(synthetic::contentCalls.load(), 2);
}
void ControllerTests::draftAttachmentsSurviveControllerRestart()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path(), 0, false));
    const QString source = root.filePath("synthetic-source.txt");
    QFile file(source);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("synthetic attachment fixture"), 28);
    file.close();
    QString id, copy;
    {
        MailController controller(false, root.path());
        QTRY_VERIFY(controller.configured());
        auto draft = controller.newDraft();
        id = draft.value("id").toString();
        const auto attachment = controller.addAttachment(QUrl::fromLocalFile(source).toString());
        QVERIFY(!attachment.isEmpty());
        copy = attachment.value("path").toString();
        QVERIFY(copy != source);
        draft.insert("attachments", QVariantList{attachment});
        draft.insert("body", QStringLiteral("durable body"));
        QVERIFY(!controller.saveDraft(draft).isEmpty());
    }
    QVERIFY(QFile::remove(source));
    MailController restarted(false, root.path());
    QTRY_VERIFY(restarted.configured());
    const auto recovered = restarted.loadDraft(id);
    QCOMPARE(recovered.value("body").toString(), QStringLiteral("durable body"));
    const auto attachments = recovered.value("attachments").toList();
    QCOMPARE(attachments.size(), 1);
    QCOMPARE(attachments.first().toMap().value("path").toString(), copy);
    QVERIFY(!attachments.first().toMap().contains("bytes"));
    QFile recoveredFile(copy);
    QVERIFY(recoveredFile.open(QIODevice::ReadOnly));
    QCOMPARE(recoveredFile.readAll(), QByteArray("synthetic attachment fixture"));
    QCOMPARE(synthetic::credentialReads.load(), 0);
}
void ControllerTests::uidValidityChangeRestartsPagination()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    const auto normalPage = synthetic::page;
    synthetic::page = [normalPage](const Account &a, const QString &folder, int offset, int limit) {
        auto p = normalPage(a, folder, offset, limit);
        if (offset > 0 || synthetic::pageCalls.load() > 2) {
            p.uidValidity = "101";
            for (auto &s : p.messages) s.uidValidity = p.uidValidity;
        }
        return p;
    };
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.refresh();
    QTRY_VERIFY(!controller.busy());
    controller.openMessage("120", "100");
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    controller.loadMore();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(offsets(), QList<int>({0, 50, 0}));
    QCOMPARE(messages(controller)->count(), 50);
    QCOMPARE(messages(controller)->get(0).value("uid").toString(), QStringLiteral("120"));
    QCOMPARE(messages(controller)->get(0).value("uidValidity").toString(), QStringLiteral("101"));
    QVERIFY(controller.selectedMessage().isEmpty());
    QVERIFY(controller.selectedUid().isEmpty());
}
void ControllerTests::staleBodySelectionCannotExportPreviousAttachment()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.openMessage("120", "100");
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    controller.disconnectAccount();
    controller.openMessage("119", "100");
    QVERIFY(controller.selectedMessage().isEmpty());
    const auto target = root.filePath("must-not-export-previous-message.txt");
    controller.saveAttachment(0, QUrl::fromLocalFile(target).toString());
    QVERIFY(!QFile::exists(target));
}
void ControllerTests::lateResultsIgnoredAfterFolderChange()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    QSemaphore release;
    const auto normal = synthetic::content;
    synthetic::content = [&release, normal](const Account &a, const QString &folder, const QString &validity, const QString &uid) {
        release.acquire();
        return normal(a, folder, validity, uid);
    };
    MailController controller(false, root.path());
    ReleaseOnExit cleanup{release};
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.openMessage("120", "100");
    QTRY_COMPARE(synthetic::contentCalls.load(), 1);
    controller.selectFolder("Archive");
    release.release();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.currentFolder(), QStringLiteral("Archive"));
    QVERIFY(controller.selectedMessage().isEmpty());
    QVERIFY(controller.selectedUid().isEmpty());
    QCOMPARE(messages(controller)->get(0).value("folder").toString(), QStringLiteral("Archive"));
}
void ControllerTests::lateCredentialCannotUndoDisconnect()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    CredentialStore *pending = nullptr;
    QString id;
    synthetic::readCredential = [&pending, &id](CredentialStore *store, const QString &accountId) { pending = store; id = accountId; };
    MailController controller(false, root.path());
    QTRY_VERIFY(pending != nullptr);
    controller.disconnectAccount();
    emit pending->passwordReady(id, QStringLiteral("late-old-password"));
    controller.refresh();
    QCOMPARE(synthetic::folderCalls.load(), 0);
    QVERIFY(controller.error().contains("password"));
    QSettings settings(root.filePath("credentials.ini"), QSettings::IniFormat);
    QVERIFY(!settings.value("restoreAllowed").toBool());
}
void ControllerTests::credentialKeysAreProfileScoped()
{
    QTemporaryDir first(temporaryTemplate()), second(temporaryTemplate());
    QVERIFY(seed(first.path()));
    QVERIFY(seed(second.path()));
    MailController one(false, first.path()), two(false, second.path());
    QTRY_COMPARE(synthetic::credentialReadKeys.size(), 2);
    QTRY_VERIFY(one.status().contains("restored"));
    QTRY_VERIFY(two.status().contains("restored"));
    const QString keyOne = synthetic::credentialReadKeys.at(0);
    const QString keyTwo = synthetic::credentialReadKeys.at(1);
    QVERIFY(keyOne != keyTwo);
    QVERIFY(keyOne != QLatin1String("default"));
    QVERIFY(keyTwo != QLatin1String("default"));
    one.configureAccount(one.account(), QStringLiteral("first-secret"), true);
    two.configureAccount(two.account(), QStringLiteral("second-secret"), true);
    QCOMPARE(synthetic::credentialWriteKeys, QStringList({keyOne, keyTwo}));
    one.configureAccount(one.account(), {}, false);
    two.configureAccount(two.account(), {}, false);
    QCOMPARE(synthetic::credentialDeleteKeys, QStringList({keyOne, keyTwo}));
    // Canonical spelling of one profile must resolve to its original key.
    MailController same(false, first.path() + QStringLiteral("/./"));
    QTRY_VERIFY(same.configured());
    same.configureAccount(same.account(), QStringLiteral("same-profile"), true);
    QCOMPARE(synthetic::credentialWriteKeys.last(), keyOne);
}
void ControllerTests::lateCredentialCompletionPreservesDisconnectTombstone()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.configureAccount(controller.account(), QStringLiteral("session-secret"), true);
    controller.disconnectAccount();
    const QString disconnected = controller.status();
    auto *store = controller.findChild<CredentialStore *>();
    QVERIFY(store != nullptr);
    const QString key = synthetic::credentialWriteKeys.last();
    emit store->passwordSaved(key);
    emit store->passwordDeleted(key);
    emit store->passwordReady(key, QStringLiteral("stale-password"));
    QCoreApplication::processEvents(); // includes queued write completion
    QCOMPARE(controller.status(), disconnected);
    QSettings settings(root.filePath("credentials.ini"), QSettings::IniFormat);
    QVERIFY(!settings.value("restoreAllowed").toBool());
    controller.refresh();
    QCOMPARE(synthetic::folderCalls.load(), 0);
}
void ControllerTests::accountTransportChangeClearsVisibleCache()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path(), 120));
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.openMessage("120", "100");
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    auto configuration = controller.account();
    configuration.insert("imapPort", 1993);
    controller.configureAccount(configuration, QStringLiteral("new-session-password"), false);
    QCOMPARE(messages(controller)->count(), 0);
    QVERIFY(controller.selectedMessage().isEmpty());
    QVERIFY(controller.selectedUid().isEmpty());
    QVERIFY(controller.folders().isEmpty());
}
void ControllerTests::staleDraftMapNeverResends()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    QSemaphore release;
    synthetic::send = [&release](const Account &, const Draft &) {
        release.acquire();
        return SendResult{SendResult::Unknown, QStringLiteral("Synthetic ambiguous outcome")};
    };
    MailController controller(false, root.path());
    ReleaseOnExit cleanup{release};
    QTRY_VERIFY(controller.status().contains("restored"));
    auto draft = controller.newDraft();
    const auto id = draft.value("id").toString();
    draft.insert("to", "nobody@example.invalid");
    controller.sendDraft(draft);
    QTRY_COMPARE(synthetic::sendCalls.load(), 1);
    QCOMPARE(controller.saveDraft(draft).value("state").toString(), QStringLiteral("sending"));
    controller.sendDraft(draft);
    QCOMPARE(synthetic::sendCalls.load(), 1);
    release.release();
    QTRY_VERIFY(!controller.sending());
    QCOMPARE(controller.loadDraft(id).value("state").toString(), QStringLiteral("unknown"));
    QCOMPARE(controller.saveDraft(draft).value("state").toString(), QStringLiteral("unknown"));
    controller.sendDraft(draft);
    QCOMPARE(synthetic::sendCalls.load(), 1);
}
void ControllerTests::interruptedSendRestoresUnknown()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    QString id;
    {
        MailDatabase db;
        QVERIFY(db.open(root.path()));
        Draft draft;
        draft.id = id = QStringLiteral("interrupted");
        draft.to = QStringLiteral("nobody@example.invalid");
        QVERIFY(db.saveDraft(draft));
        QVERIFY(db.markDraftState(id, "sending"));
    }
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    const auto draft = controller.loadDraft(id);
    QCOMPARE(draft.value("state").toString(), QStringLiteral("unknown"));
    controller.sendDraft(draft);
    QCOMPARE(synthetic::sendCalls.load(), 0);
}
void ControllerTests::composeReplyAndForward()
{
    QTemporaryDir root(temporaryTemplate());
    MailController controller(true, root.path());
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    QSignalSpy changed(&controller, &MailController::draftsChanged);
    const auto fresh = controller.newDraft("new");
    QVERIFY(!fresh.value("id").toString().isEmpty());
    QVERIFY(fresh.value("to").toString().isEmpty());
    QVERIFY(fresh.value("subject").toString().isEmpty());
    QVERIFY(fresh.value("body").toString().isEmpty());
    QCOMPARE(fresh.value("state").toString(), QStringLiteral("draft"));
    const auto reply = controller.newDraft("reply");
    QCOMPARE(reply.value("to").toString(), QStringLiteral("hello1@example.invalid"));
    QCOMPARE(reply.value("subject").toString(), QStringLiteral("Re: A quieter place for your inbox"));
    QCOMPARE(reply.value("inReplyTo").toString(), QStringLiteral("<demo-0@example.invalid>"));
    QCOMPARE(reply.value("references"), reply.value("inReplyTo"));
    QVERIFY(reply.value("body").toString().contains("\n> Hi there,\n> \n"));
    QVERIFY(reply.value("attachments").toList().isEmpty());
    const auto all = controller.newDraft("replyAll");
    QCOMPARE(all.value("to"), reply.value("to"));
    QVERIFY(all.value("cc").toString().isEmpty());
    const auto forward = controller.newDraft("forward");
    QVERIFY(forward.value("to").toString().isEmpty());
    QCOMPARE(forward.value("subject").toString(), QStringLiteral("Fwd: A quieter place for your inbox"));
    QVERIFY(forward.value("inReplyTo").toString().isEmpty());
    QVERIFY(forward.value("references").toString().isEmpty());
    const auto attachments = forward.value("attachments").toList();
    QCOMPARE(attachments.size(), 1);
    const auto metadata = attachments.first().toMap();
    QCOMPARE(metadata.value("name").toString(), QStringLiteral("Design direction.txt"));
    QVERIFY(!metadata.contains("bytes"));
    QFile copied(metadata.value("path").toString());
    QVERIFY(copied.open(QIODevice::ReadOnly));
    QCOMPARE(copied.readAll(), QByteArray("A calmer inbox. Clear hierarchy. Familiar interactions.\n"));
    QCOMPARE(controller.loadDraft(forward.value("id").toString()), forward);
    QCOMPARE(changed.count(), 4);
    // Subject prefixes are not repeatedly added.
    controller.openMessage("95", "20261029");
    const auto prefixed = controller.newDraft("reply");
    QCOMPARE(prefixed.value("subject").toString(), QStringLiteral("Re: The little things"));
}
void ControllerTests::replyAllQuotedNamesAndExactSelfExclusion()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    const auto normal = synthetic::content;
    synthetic::content = [normal](const Account &a, const QString &folder, const QString &validity, const QString &uid) {
        auto c = normal(a, folder, validity, uid);
        c.summary.senderAddress = QStringLiteral("sender@example.invalid");
        c.replyTo = QStringLiteral("\"Sender, Team\" <sender@example.invalid>");
        c.to = QStringLiteral("\"Test, User\" <TEST@example.invalid>, \"Doe, Jane\" <jane@example.invalid>, nottest@example.invalid");
        c.cc = QStringLiteral("Jane again <JANE@example.invalid>, Sender <sender@example.invalid>, other@example.invalid");
        return c;
    };
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.openMessage("120", "100");
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    const auto reply = controller.newDraft("replyAll");
    QVERIFY(!reply.isEmpty());
    QCOMPARE(reply.value("cc").toString(), QStringLiteral("jane@example.invalid, nottest@example.invalid, other@example.invalid"));
    QCOMPARE(reply.value("to").toString(), QStringLiteral("sender@example.invalid"));
    QCOMPARE(synthetic::sendCalls.load(), 0);
}
void ControllerTests::unsupportedReplyRecipientsFailClosed()
{
    QTemporaryDir root(temporaryTemplate());
    QVERIFY(seed(root.path()));
    const auto normal = synthetic::content;
    synthetic::content = [normal](const Account &a, const QString &folder, const QString &validity, const QString &uid) {
        auto c = normal(a, folder, validity, uid);
        c.summary.senderAddress = QStringLiteral("sender@example.invalid");
        c.replyTo = QStringLiteral("sender@example.invalid");
        c.to = QStringLiteral("Friends: jane@example.invalid, bob@example.invalid;");
        return c;
    };
    MailController controller(false, root.path());
    QTRY_VERIFY(controller.status().contains("restored"));
    controller.openMessage("120", "100");
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    const auto before = controller.drafts().size();
    QVERIFY(controller.newDraft("replyAll").isEmpty());
    QVERIFY(!controller.error().isEmpty());
    QCOMPARE(controller.drafts().size(), before);
    // Reply uses only Reply-To; an unsupported To group does not block it.
    QVERIFY(!controller.newDraft("reply").isEmpty());
    QVERIFY(controller.newDraft("unsupported-mode").isEmpty());
}
void ControllerTests::attachmentExportAndDraftDiscard()
{
    QTemporaryDir root(temporaryTemplate());
    MailController controller(true, root.path());
    QTRY_VERIFY(!controller.selectedMessage().isEmpty());
    const auto exported = root.filePath("exported.txt");
    controller.saveAttachment(0, QUrl::fromLocalFile(exported).toString());
    QFile file(exported);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto original = file.readAll();
    file.close();
    QCOMPARE(original, QByteArray("A calmer inbox. Clear hierarchy. Familiar interactions.\n"));
    controller.saveAttachment(0, QUrl::fromLocalFile(exported).toString());
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
    file.close();
    controller.saveAttachment(-1, QUrl::fromLocalFile(root.filePath("invalid-index.txt")).toString());
    QVERIFY(!QFile::exists(root.filePath("invalid-index.txt")));
    QVERIFY(controller.addAttachment("https://example.invalid/fixture.txt").isEmpty());
    QVERIFY(controller.addAttachment(QUrl::fromLocalFile(root.filePath("missing")).toString()).isEmpty());
    QFile large(root.filePath("oversized.bin"));
    QVERIFY(large.open(QIODevice::WriteOnly));
    QVERIFY(large.resize(15 * 1024 * 1024 + 1));
    large.close();
    QVERIFY(controller.addAttachment(QUrl::fromLocalFile(large.fileName()).toString()).isEmpty());
    const auto attached = controller.addAttachment(QUrl::fromLocalFile(exported).toString());
    QVERIFY(!attached.isEmpty());
    auto draft = controller.newDraft();
    draft.insert("attachments", QVariantList{attached});
    QVERIFY(!controller.saveDraft(draft).isEmpty());
    const QString id = draft.value("id").toString();
    QSignalSpy changed(&controller, &MailController::draftsChanged);
    controller.discardDraft(id);
    QVERIFY(controller.loadDraft(id).isEmpty());
    QCOMPARE(changed.count(), 1);
    QVERIFY(QFile::exists(exported));
    QVERIFY(QFile::exists(attached.value("path").toString()));
    QCOMPARE(synthetic::sendCalls.load(), 0);
    QCOMPARE(synthetic::credentialReads.load(), 0);
}
QTEST_GUILESS_MAIN(ControllerTests)
#include "tst_controller.moc"
