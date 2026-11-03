#pragma once
#include <QQuickWebEngineProfile>

class MailDocumentHandler;
void registerMailDocumentScheme();

// Mail pages never use the default browser profile or Chromium's network stack.
class MailWebProfile : public QQuickWebEngineProfile {
    Q_OBJECT
public:
    explicit MailWebProfile(QObject *parent = nullptr);
    Q_INVOKABLE QUrl setDocument(const QString &html);
private:
    MailDocumentHandler *m_documents;
};
