#include "platform/TrayService.h"
#include <QGuiApplication>
#include <QSystemTrayIcon>
#include <QImage>
#include <QSignalSpy>
#include <QtTest>
using namespace nkmail;
class FakeTray final : public TrayBackend {
public:
    int unread = -1;
    void setUnreadCount(int count) override { unread = count; }
    bool present = true;
    bool available() const override { return present; }
};
class TrayTests : public QObject {
    Q_OBJECT
private slots:
    void nativeTrayBadgeUpdatesAndClears() {
        auto backend = createSystemTray();
        auto *icon = backend->findChild<QSystemTrayIcon *>(); QVERIFY(icon);
        backend->setUnreadCount(0); const auto clear = icon->icon().pixmap(64, 64).toImage();
        backend->setUnreadCount(12); const auto twelve = icon->icon().pixmap(64, 64).toImage();
        QVERIFY(twelve != clear); QVERIFY(icon->toolTip().contains("12"));
        backend->setUnreadCount(123); const auto many = icon->icon().pixmap(64, 64).toImage();
        QVERIFY(many != twelve); QVERIFY(icon->toolTip().contains("123"));
        const auto capture = qEnvironmentVariable("NKMAIL_TRAY_CAPTURE");
        if (!capture.isEmpty()) {
            QVERIFY(twelve.save(capture + "-12.png")); QVERIFY(many.save(capture + "-99plus.png"));
        }
        backend->setUnreadCount(0); QCOMPARE(icon->icon().pixmap(64, 64).toImage(), clear);
    }
    void unreadCountsReachBackendAndClear() {
        auto backend = std::make_unique<FakeTray>(); auto *tray = backend.get();
        TrayService service(std::move(backend));
        service.setUnreadCount(123); QCOMPARE(tray->unread, 123);
        service.setUnreadCount(0); QCOMPARE(tray->unread, 0);
        service.setUnreadCount(-1); QCOMPARE(tray->unread, 0);
    }
    void closeHidesAndActivationRestores() {
        QWindow window; window.show();
        auto backend = std::make_unique<FakeTray>(); auto *tray = backend.get();
        TrayService service(std::move(backend));
        service.attach(&window, [] { return true; });
        QVERIFY(!QGuiApplication::quitOnLastWindowClosed());
        QVERIFY(service.hideToTray()); QVERIFY(!window.isVisible());
        emit tray->showRequested(); QVERIFY(window.isVisible());
        QVERIFY(service.hideToTray());
        emit tray->showRequested(); QVERIFY(window.isVisible());
    }
    void missingTrayDoesNotHideWindow() {
        QWindow window; window.show();
        auto backend = std::make_unique<FakeTray>(); backend->present = false;
        TrayService service(std::move(backend));
        service.attach(&window, [] { return true; });
        QVERIFY(!service.available()); QVERIFY(!service.hideToTray());
        QVERIFY(window.isVisible());
    }
    void disappearingHostRestoresHiddenWindow() {
        QWindow window; window.show();
        auto backend = std::make_unique<FakeTray>(); auto *tray = backend.get();
        TrayService service(std::move(backend)); service.attach(&window, [] { return true; });
        QVERIFY(service.hideToTray()); tray->present = false; service.refreshAvailability();
        QVERIFY(window.isVisible()); QVERIFY(!service.available());
        QVERIFY(QGuiApplication::quitOnLastWindowClosed());
        tray->present = true; service.refreshAvailability();
        QVERIFY(service.hideToTray());
    }
    void explicitExitRespectsGuard() {
        QWindow window; window.show();
        auto backend = std::make_unique<FakeTray>(); auto *tray = backend.get();
        TrayService service(std::move(backend));
        bool safe = false; int checks = 0;
        service.attach(&window, [&] { ++checks; return safe; });
        QSignalSpy exited(&service, &TrayService::exitRequested);
        service.hideToTray(); emit tray->quitRequested();
        QCOMPARE(checks, 1); QCOMPARE(exited.count(), 0); QVERIFY(window.isVisible());
        safe = true; emit tray->quitRequested();
        QCOMPARE(checks, 2); QCOMPARE(exited.count(), 1);
    }
};
QTEST_MAIN(TrayTests)
#include "tst_tray.moc"
