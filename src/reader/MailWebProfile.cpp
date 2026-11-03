#include "MailWebProfile.h"
#include <QCoreApplication>
#include <QBuffer>
#include <QUuid>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineCookieStore>
#include <QtQml/qqml.h>

void registerMailDocumentScheme() {
    QWebEngineUrlScheme scheme("nkmail-body");
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::NoAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

class MailDocumentHandler final : public QWebEngineUrlSchemeHandler {
public:
    using QWebEngineUrlSchemeHandler::QWebEngineUrlSchemeHandler;
    QUrl setDocument(const QString &html) {
        m_url = QUrl(QStringLiteral("nkmail-body://message/") + QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_html = html.toUtf8();
        return m_url;
    }
    void requestStarted(QWebEngineUrlRequestJob *job) override {
        if (job->requestUrl() != m_url || job->requestMethod() != QByteArrayLiteral("GET")) {
            job->fail(QWebEngineUrlRequestJob::RequestDenied);
            return;
        }
        auto *buffer = new QBuffer(job);
        buffer->setData(m_html);
        buffer->open(QIODevice::ReadOnly);
        job->reply(QByteArrayLiteral("text/html"), buffer);
    }
private:
    QUrl m_url;
    QByteArray m_html;
};

namespace {
class MailRequests final : public QWebEngineUrlRequestInterceptor {
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        const auto url = info.requestUrl();
        // Inline document/image bytes are supplied by NkMail. No HTTP, file,
        // qrc, blob, custom-protocol or subframe request may leave this profile.
        const bool inlineDocument = url.scheme() == QLatin1String("nkmail-body")
            && info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame;
        const bool inlineImage = url.scheme() == QLatin1String("data")
            && info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeImage;
        const bool blank = url == QUrl(QStringLiteral("about:blank"))
            && info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame;
        info.block(!(inlineDocument || inlineImage || blank));
    }
};
void registerMailWebProfile() {
    qmlRegisterType<MailWebProfile>("NextKde.Mail.Reader", 1, 0, "MailWebProfile");
}
}
Q_COREAPP_STARTUP_FUNCTION(registerMailWebProfile)

MailWebProfile::MailWebProfile(QObject *parent) : QQuickWebEngineProfile(parent) {
    setOffTheRecord(true);
    setHttpCacheType(QQuickWebEngineProfile::NoCache);
    setPersistentCookiesPolicy(QQuickWebEngineProfile::NoPersistentCookies);
    setPushServiceEnabled(false);
    cookieStore()->setCookieFilter([](const QWebEngineCookieStore::FilterRequest &) { return false; });
    setUrlRequestInterceptor(new MailRequests(this));
    m_documents = new MailDocumentHandler(this);
    installUrlSchemeHandler(QByteArrayLiteral("nkmail-body"), m_documents);
}

QUrl MailWebProfile::setDocument(const QString &html) { return m_documents->setDocument(html); }
