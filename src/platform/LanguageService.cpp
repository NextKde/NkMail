#include "LanguageService.h"
#include <QCoreApplication>
#include <QLocale>
#include <QLibraryInfo>

namespace nkmail {
LanguageService::LanguageService(const QString &settingsPath, QObject *parent)
    : QObject(parent), m_settings(settingsPath, QSettings::IniFormat) {
    auto choice = m_settings.value("language", "system").toString();
    if (choice != "system" && choice != "zh_CN" && choice != "en") choice = "system";
    setLanguage(choice);
}
LanguageService::~LanguageService() {
    QCoreApplication::removeTranslator(&m_translator);
    QCoreApplication::removeTranslator(&m_qtBase);
    QCoreApplication::removeTranslator(&m_qtDeclarative);
}
void LanguageService::setLanguage(const QString &language) {
    if ((language != "system" && language != "zh_CN" && language != "en") || language == m_language) return;
    const QLocale selected = language == "system" ? QLocale::system() : QLocale(language);
    const QString catalog = selected.language() == QLocale::Chinese ? "zh_CN" : "en";
    QCoreApplication::removeTranslator(&m_translator);
    QCoreApplication::removeTranslator(&m_qtBase);
    QCoreApplication::removeTranslator(&m_qtDeclarative);
    const auto qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (m_qtBase.load(QLocale(catalog), "qtbase", "_", qtPath))
        QCoreApplication::installTranslator(&m_qtBase);
    if (m_qtDeclarative.load(QLocale(catalog), "qtdeclarative", "_", qtPath))
        QCoreApplication::installTranslator(&m_qtDeclarative);
    if (!m_translator.load(QStringLiteral(":/i18n/nkmail_%1.qm").arg(catalog)))
        qFatal("NkMail translation catalog is missing");
    QCoreApplication::installTranslator(&m_translator);
    QLocale::setDefault(selected);
    m_language = language;
    m_localeName = selected.name();
    m_settings.setValue("language", language);
    emit languageChanged();
}
}
