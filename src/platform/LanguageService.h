#pragma once
#include <QObject>
#include <QTranslator>
#include <QSettings>

namespace nkmail {
class LanguageService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString localeName READ localeName NOTIFY languageChanged)
public:
    explicit LanguageService(const QString &settingsPath, QObject *parent = nullptr);
    ~LanguageService() override;
    QString language() const { return m_language; }
    QString localeName() const { return m_localeName; }
    void setLanguage(const QString &language);
signals:
    void languageChanged();
private:
    QSettings m_settings;
    QTranslator m_translator;
    QTranslator m_qtBase;
    QTranslator m_qtDeclarative;
    QString m_language;
    QString m_localeName;
};
}
