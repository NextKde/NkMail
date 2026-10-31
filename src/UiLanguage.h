#pragma once

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace nkmail {

// Keep translators alive for as long as the application and its QML engine.
class UiLanguage final {
public:
    bool install() {
        QLocale::setDefault(QLocale(QStringLiteral("zh_CN")));
        return load(m_base, ":/i18n/qtbase_zh_CN.qm")
            && load(m_declarative, ":/i18n/qtdeclarative_zh_CN.qm")
            && load(m_application, ":/i18n/nkmail_zh_CN.qm");
    }

private:
    static bool load(QTranslator &translator, const char *path) {
        return translator.load(QString::fromLatin1(path))
            && QCoreApplication::installTranslator(&translator);
    }
    QTranslator m_base;
    QTranslator m_declarative;
    QTranslator m_application;
};

} // namespace nkmail
