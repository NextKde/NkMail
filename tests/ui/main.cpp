#include <QtQuickTest/quicktest.h>
#include <QGuiApplication>
#include <QQuickStyle>

int main(int argc, char **argv)
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    return quick_test_main(argc, argv, "nkmail_ui", QUICK_TEST_SOURCE_DIR);
}
