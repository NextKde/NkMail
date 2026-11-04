#include <QtQuickTest/quicktest.h>
#include <QGuiApplication>
#include <QtWebEngineQuick>
#include "../../src/reader/MailWebProfile.h"
#include "ReaderTestServer.h"
#include <QtQml/qqml.h>
#include <QQuickStyle>

int main(int argc, char **argv)
{
    registerMailDocumentScheme();
    QtWebEngineQuick::initialize();
    qmlRegisterType<ReaderTestServer>("NextKde.Mail.ReaderTests", 1, 0, "ReaderTestServer");
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    return quick_test_main(argc, argv, "nkmail_ui", QUICK_TEST_SOURCE_DIR);
}
