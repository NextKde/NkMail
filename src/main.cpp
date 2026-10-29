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
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

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
    engine.setInitialProperties({{QStringLiteral("mail"), QVariant::fromValue(&mail)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("NextKde.Mail"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (demo && window) {
        window->resize(qBound(760, parser.value("review-width").toInt(), 2000), qBound(520, parser.value("review-height").toInt(), 1600));
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
