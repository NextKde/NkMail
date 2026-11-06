#include "services/MailAlerts.h"
#include "NotificationService.h"
#include <QDBusConnection>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
using namespace nkmail;
class Desktop final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    uint next = 0;
    bool markup = true;
    QStringList actions;
    QString body;
public slots:
    QStringList GetCapabilities() { return markup ? QStringList{"actions", "body", "body-markup"} : QStringList{"actions", "body"}; }
    uint Notify(const QString &, uint, const QString &, const QString &, const QString &text,
                const QStringList &buttons, const QVariantMap &, int) {
        actions = buttons; body = text; return ++next;
    }
signals:
    void ActionInvoked(uint id, const QString &action);
    void NotificationClosed(uint id, uint reason);
};
class AlertsTests : public QObject {
    Q_OBJECT
    static Account account(const QString &protocol = "imap") {
        Account a; a.id = "one"; a.incomingProtocol = protocol; a.username = a.address = "one@example.invalid";
        a.imapHost = "mail.example.invalid"; return a;
    }
    static Page page(const QString &uid, const QString &epoch = "epoch") {
        Page p; p.folder = "INBOX"; p.uidValidity = epoch;
        Summary s; s.folder = p.folder; s.uidValidity = epoch; s.uid = uid; s.unread = true; s.subject = "Synthetic notice";
        p.messages = {s}; return p;
    }
private slots:
    void imapBaselinesDeduplicateAndSurviveRestart() {
        QTemporaryDir dir;
        {
            MailAlerts alerts(dir.path()); QSignalSpy notices(&alerts, &MailAlerts::newMail);
            alerts.observe(account(), page("10")); QCOMPARE(notices.count(), 0);
            alerts.observe(account(), page("11")); QCOMPARE(notices.count(), 1);
            QCOMPARE(notices[0][2].toMap().value("uid").toString(), "11");
            alerts.observe(account(), page("11")); QCOMPARE(notices.count(), 1);
            alerts.observe(account(), page("1")); alerts.observe(account(), page("11")); QCOMPARE(notices.count(), 1);
        }
        MailAlerts alerts(dir.path()); QSignalSpy notices(&alerts, &MailAlerts::newMail);
        alerts.observe(account(), page("11")); QCOMPARE(notices.count(), 0);
        alerts.observe(account(), page("12")); QCOMPARE(notices.count(), 1);
        alerts.observe(account(), page("999", "new-epoch")); QCOMPARE(notices.count(), 1);
        auto edited = account(); edited.imapHost = "different.example.invalid";
        alerts.observe(edited, page("1000", "new-epoch")); QCOMPARE(notices.count(), 1);
    }
    void popUsesOpaqueIdentitiesAndDoesNotAnnounceOldHeaders() {
        QTemporaryDir dir; MailAlerts alerts(dir.path()); QSignalSpy notices(&alerts, &MailAlerts::newMail);
        auto first = page("Z-newest"); first.inboxIdentities = {"old-not-downloaded", "Z-newest"};
        alerts.observe(account("pop3"), first);
        auto old = page("old-not-downloaded"); old.inboxIdentities = {"old-not-downloaded"};
        alerts.observe(account("pop3"), old); QCOMPARE(notices.count(), 0);
        auto incoming = page("A-opaque-new"); incoming.inboxIdentities = {"old-not-downloaded", "A-opaque-new"};
        alerts.observe(account("pop3"), incoming); QCOMPARE(notices.count(), 1);
        MailAlerts restarted(dir.path()); QSignalSpy later(&restarted, &MailAlerts::newMail);
        restarted.observe(account("pop3"), incoming); QCOMPARE(later.count(), 0);
    }
    void disabledAdvancesCursorsAndPersistsPerProfile() {
        QTemporaryDir dir, other;
        { MailAlerts alerts(dir.path()); alerts.observe(account(), page("1")); alerts.setEnabled(false); }
        MailAlerts alerts(dir.path()); QVERIFY(!alerts.enabled());
        MailAlerts isolated(other.path()); QVERIFY(isolated.enabled());
        QSignalSpy notices(&alerts, &MailAlerts::newMail);
        alerts.observe(account(), page("2")); alerts.setEnabled(true); alerts.observe(account(), page("2"));
        QCOMPARE(notices.count(), 0);
        auto read = page("3"); read.messages[0].unread = false; alerts.observe(account(), read);
        QCOMPARE(notices.count(), 0);
        alerts.observe(account(), page("4")); QCOMPARE(notices.count(), 1);
        alerts.forget("one"); alerts.observe(account(), page("5")); QCOMPARE(notices.count(), 1);
        auto second = account(); second.id = "two";
        alerts.observe(second, page("5")); alerts.observe(second, page("6")); QCOMPARE(notices.count(), 2);
        QCOMPARE(notices.last()[2].toMap().value("accountId").toString(), "two");
    }
    void desktopActionsAreBoundToTheReturnedNotificationId() {
        Desktop desktop; auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService("org.freedesktop.Notifications"));
        QVERIFY(bus.registerObject("/org/freedesktop/Notifications", &desktop,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        NotificationService notifications; QSignalSpy activated(&notifications, &NotificationService::activated);
        const QVariantMap one{{"uid", "one"}}, two{{"uid", "two"}};
        notifications.notify("Synthetic", "<b>sender text</b>", one);
        QTRY_COMPARE(desktop.next, 1u); QTest::qWait(20);
        QVERIFY(desktop.actions.contains("default")); QCOMPARE(desktop.body, "&lt;b&gt;sender text&lt;/b&gt;");
        notifications.notify("Synthetic", "Two", two); QTRY_COMPARE(desktop.next, 2u); QTest::qWait(20);
        emit desktop.ActionInvoked(1, "unknown"); emit desktop.ActionInvoked(999, "default");
        emit desktop.ActionInvoked(2, "default"); QTRY_COMPARE(activated.count(), 1); QCOMPARE(activated[0][0].toMap(), two);
        emit desktop.NotificationClosed(1, 2); QTest::qWait(20);
        emit desktop.ActionInvoked(1, "default"); QTest::qWait(20); QCOMPARE(activated.count(), 1);
        notifications.notify("Synthetic", "Before service restart", one);
        QTRY_COMPARE(desktop.next, 3u); QTest::qWait(20);
        bus.unregisterService("org.freedesktop.Notifications"); QTest::qWait(30);
        QVERIFY(bus.registerService("org.freedesktop.Notifications")); QTest::qWait(30);
        emit desktop.ActionInvoked(3, "default"); QTest::qWait(20); QCOMPARE(activated.count(), 1);
        notifications.notify("Synthetic", "After service restart", two);
        QTRY_COMPARE(desktop.next, 4u); QTest::qWait(20);
        emit desktop.ActionInvoked(4, "default"); QTRY_COMPARE(activated.count(), 2);
        QCOMPARE(activated.last()[0].toMap(), two);
        desktop.markup = false;
        notifications.notify("Synthetic", "<plain subject>", one);
        QTRY_COMPARE(desktop.next, 5u); QCOMPARE(desktop.body, "<plain subject>");
        bus.unregisterObject("/org/freedesktop/Notifications"); bus.unregisterService("org.freedesktop.Notifications");
    }
};
QTEST_GUILESS_MAIN(AlertsTests)
#include "tst_notifications.moc"
