#include "platform/LanguageService.h"
#include "platform/TrayService.h"
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QTemporaryDir>
#include <QLocale>
using nkmail::LanguageService;
class LanguageTests : public QObject {
    Q_OBJECT
private slots:
    void trayRetranslatesInPlace() {
        QTemporaryDir dir;
        LanguageService language(dir.filePath("language.ini"));
        language.setLanguage("en");
        auto backend = nkmail::createSystemTray();
        QAction *quit = nullptr;
        for (auto *widget : QApplication::allWidgets()) {
            if (auto *menu = qobject_cast<QMenu *>(widget)) {
                for (auto *action : menu->actions())
                    if (action->objectName() == "quitMail") quit = action;
            }
        }
        QVERIFY(quit);
        QCOMPARE(quit->text(), QString("Quit NkMail"));
        language.setLanguage("zh_CN");
        QTRY_COMPARE(quit->text(), QString::fromUtf8("退出 NkMail"));
        language.setLanguage("en");
        QTRY_COMPARE(quit->text(), QString("Quit NkMail"));
    }
    void switchingAndPersistence() {
        QTemporaryDir dir;
        const auto settings = dir.filePath("language.ini");
        {
            LanguageService language(settings);
            QCOMPARE(language.language(), QString("system"));
            QQmlEngine engine;
            connect(&language, &LanguageService::languageChanged, &engine, &QQmlEngine::retranslate);
            QQmlComponent component(&engine);
            component.setData("import QtQml; QtObject { property string caption: qsTranslate('Main', 'Mail') }", QUrl());
            QScopedPointer<QObject> object(component.create());
            QVERIFY2(object, qPrintable(component.errorString()));
            language.setLanguage("zh_CN");
            QCOMPARE(object->property("caption").toString(), QString::fromUtf8("邮件"));
            QCOMPARE(QCoreApplication::translate("SystemTray", "Quit NkMail"), QString::fromUtf8("退出 NkMail"));
            QCOMPARE(QCoreApplication::translate("MailErrors", "Invalid mail field."), QString::fromUtf8("邮件字段无效。"));
            language.setLanguage("en");
            QCOMPARE(object->property("caption").toString(), QString("Mail"));
            QCOMPARE(QCoreApplication::translate("MailToolbar", "撰写"), QString("Compose"));
            QCOMPARE(QCoreApplication::translate("SystemTray", "Quit NkMail"), QString("Quit NkMail"));
            language.setLanguage("invalid");
            QCOMPARE(language.language(), QString("en"));
            language.setLanguage("zh_CN");
        }
        LanguageService restored(settings);
        QCOMPARE(restored.language(), QString("zh_CN"));
        restored.setLanguage("system");
        QCOMPARE(restored.localeName(), QLocale::system().name());
    }
};
QTEST_MAIN(LanguageTests)
#include "tst_i18n.moc"
