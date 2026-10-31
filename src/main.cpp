#include "MailController.h"
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQmlProperty>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <memory>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("nkmail"));
    app.setApplicationDisplayName(QStringLiteral("NkMail"));
    app.setOrganizationName(QStringLiteral("NextKde"));
    app.setApplicationVersion(QStringLiteral(NKMAIL_VERSION));
    app.setDesktopFileName(QStringLiteral("nkmail"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQuickWindow::setDefaultAlphaBuffer(true);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A quiet, native mail client powered by Kos.Ui and mailio."));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"demo", "Use isolated synthetic mail; no network or saved credentials."});
    parser.addOption({"data-dir", "Use a separate local data directory.", "path"});
    parser.addOption({"screenshot", "Capture this application's synthetic preview window and exit (requires --demo).", "path"});
    parser.addOption({"review-scenario", "Synthetic review: inbox, compose, account or details.", "name", "inbox"});
    parser.addOption({"appearance", "Review appearance: light or dark.", "name", "light"});
    parser.addOption({"review-width", "Synthetic preview window width.", "pixels", "1240"});
    parser.addOption({"review-height", "Synthetic preview window height.", "pixels", "800"});
    parser.process(app);
    const bool demo = parser.isSet("demo");
    if (parser.isSet("screenshot") && !demo) {
        std::fprintf(stderr, "Screenshot export is only permitted with isolated --demo data.\n"); return 2;
    }
    QTemporaryDir demoDirectory;
    QString root = parser.isSet("data-dir") ? parser.value("data-dir") : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (demo) {
        if (!demoDirectory.isValid()) { std::fprintf(stderr, "Cannot create private preview storage.\n"); return 2; }
        root = demoDirectory.path();
    }
    QDir().mkpath(root);
    QFile::setPermissions(root, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QLockFile dataLock(QDir(root).filePath("nkmail.lock"));
    if (!dataLock.tryLock(0)) {
        std::fprintf(stderr, "This mailbox data directory is already open in another NkMail instance.\n"); return 2;
    }
    nkmail::MailController mail(demo, root);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("reviewAppearance"), demo ? parser.value("appearance") : QStringLiteral("system"));
    engine.rootContext()->setContextProperty(QStringLiteral("forceGlassPreview"),
        qEnvironmentVariableIsSet("NKMAIL_FORCE_GLASS"));
    // Window preferences follow the mailbox data directory. Demo uses its private
    // temporary directory; screenshot reviews never read or write preferences.
    const bool persistWindow = !parser.isSet("screenshot");
    std::unique_ptr<QSettings> windowSettings;
    QVariantMap initialProperties{{QStringLiteral("mail"), QVariant::fromValue(&mail)}};
    if (persistWindow) {
        windowSettings = std::make_unique<QSettings>(QDir(root).filePath("window.ini"), QSettings::IniFormat);
        initialProperties.insert(QStringLiteral("sidebarPreferredWidth"), qBound(190, windowSettings->value("sidebarWidth", 236).toInt(), 340));
        initialProperties.insert(QStringLiteral("listPreferredWidth"), qBound(260, windowSettings->value("listWidth", 320).toInt(), 520));
    }
    engine.setInitialProperties(initialProperties);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("NextKde.Mail"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QRect normalGeometry;
    bool maximized = false;
    QTimer preferenceSave;
    preferenceSave.setSingleShot(true);
    preferenceSave.setInterval(350);
    if (window && windowSettings) {
        auto *screen = window->screen() ? window->screen() : app.primaryScreen();
        const QRect fallback = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 820);
        QRect restored(windowSettings->value("x", fallback.x() + 40).toInt(),
                       windowSettings->value("y", fallback.y() + 40).toInt(),
                       windowSettings->value("width", 1280).toInt(),
                       windowSettings->value("height", 820).toInt());
        // Restore to the screen with the greatest intersection; unplugged screens
        // fall back to the current screen rather than stranding a frameless window.
        QRect available = fallback;
        qint64 overlap = 0;
        for (auto *candidate : app.screens()) {
            const QRect intersection = candidate->availableGeometry().intersected(restored);
            const qint64 area = qint64(intersection.width()) * intersection.height();
            if (!intersection.isEmpty() && area > overlap) { overlap = area; available = candidate->availableGeometry(); }
        }
        restored.setSize(QSize(qBound(qMin(580, available.width()), restored.width(), available.width()),
                               qBound(qMin(520, available.height()), restored.height(), available.height())));
        restored.moveLeft(qBound(available.left(), restored.x(), available.right() - restored.width() + 1));
        restored.moveTop(qBound(available.top(), restored.y(), available.bottom() - restored.height() + 1));
        window->setGeometry(restored);
        normalGeometry = restored;
        maximized = windowSettings->value("maximized", false).toBool();
        if (maximized) window->showMaximized();
        const auto savePreferences = [&] {
            windowSettings->setValue("x", normalGeometry.x()); windowSettings->setValue("y", normalGeometry.y());
            windowSettings->setValue("width", normalGeometry.width()); windowSettings->setValue("height", normalGeometry.height());
            windowSettings->setValue("maximized", maximized);
            windowSettings->setValue("sidebarWidth", window->property("sidebarPreferredWidth"));
            windowSettings->setValue("listWidth", window->property("listPreferredWidth"));
            windowSettings->sync();
            QFile::setPermissions(windowSettings->fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        };
        QObject::connect(&preferenceSave, &QTimer::timeout, &preferenceSave, savePreferences);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &preferenceSave, savePreferences);
        const auto geometryChanged = [&] {
            if (window->visibility() == QWindow::Windowed) normalGeometry = window->geometry();
            preferenceSave.start();
        };
        QObject::connect(window, &QWindow::xChanged, &preferenceSave, geometryChanged);
        QObject::connect(window, &QWindow::yChanged, &preferenceSave, geometryChanged);
        QObject::connect(window, &QWindow::widthChanged, &preferenceSave, geometryChanged);
        QObject::connect(window, &QWindow::heightChanged, &preferenceSave, geometryChanged);
        QObject::connect(window, &QWindow::visibilityChanged, &preferenceSave, [&](QWindow::Visibility visibility) {
            if (visibility == QWindow::Maximized) maximized = true;
            else if (visibility == QWindow::Windowed) maximized = false;
            preferenceSave.start();
        });
        // QQmlProperty bridges QML-generated notify signals to the debounce timer.
        QQmlProperty(window, "sidebarPreferredWidth").connectNotifySignal(&preferenceSave, SLOT(start()));
        QQmlProperty(window, "listPreferredWidth").connectNotifySignal(&preferenceSave, SLOT(start()));
    }
    if (demo && window && (parser.isSet("screenshot") || parser.isSet("review-width") || parser.isSet("review-height"))) {
        window->resize(qBound(580, parser.value("review-width").toInt(), 2000), qBound(520, parser.value("review-height").toInt(), 1600));
    }
    if (parser.isSet("screenshot")) {
        const QString path = parser.value("screenshot");
        const QString scenario = parser.value("review-scenario");
        QTimer::singleShot(1200, &app, [window, scenario] {
            if (window) QMetaObject::invokeMethod(window, "showReview", Q_ARG(QVariant, QVariant(scenario)));
        });
        QTimer::singleShot(3000, &app, [window, path, &app] {
            if (!window || !window->isExposed()) { std::fprintf(stderr, "Preview window is not exposed.\n"); app.exit(3); return; }
            const QImage image = window->grabWindow();
            if (image.isNull()) { std::fprintf(stderr, "Application screenshot capture failed.\n"); app.exit(3); return; }
            QDir().mkpath(QFileInfo(path).absolutePath());
            if (!image.save(path)) { std::fprintf(stderr, "Cannot save application screenshot.\n"); app.exit(3); return; }
            std::fprintf(stdout, "Synthetic application screenshot saved.\n"); app.exit(0);
        });
    }
    return app.exec();
}
