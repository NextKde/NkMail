#include <QtQuickTest/quicktest.h>
#include <QGuiApplication>
#include <QQuickStyle>
#include "../../src/UiLanguage.h"

class UiSetup final : public QObject {
    Q_OBJECT
public slots:
    void applicationAvailable() {
        if (!m_language.install()) qFatal("Cannot load embedded Chinese translations for UI tests.");
    }
private:
    nkmail::UiLanguage m_language;
};

int main(int argc, char **argv)
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    UiSetup setup;
    return quick_test_main_with_setup(argc, argv, "nkmail_ui", QUICK_TEST_SOURCE_DIR, &setup);
}

#include "main.moc"
