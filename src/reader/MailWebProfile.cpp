#include "MailWebProfile.h"
#include <QCoreApplication>
#include <QBuffer>
#include <QTimer>
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
    qmlRegisterType<MailRenderLifecycle>("NextKde.Mail.Reader", 1, 0, "MailRenderLifecycle");
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

void MailRenderLifecycle::setView(QQuickItem *view)
{
    if (m_view == view) return;
    for (auto *item : std::as_const(m_watched)) QObject::disconnect(item, nullptr, this, nullptr);
    m_watched.clear();
    m_retained.clear();
    m_view = view;
    if (m_attached) { m_attached = false; emit attachedChanged(); }
    emit viewChanged();
    scheduleScan();
}

void MailRenderLifecycle::scheduleScan()
{
    if (m_scheduled) return;
    m_scheduled = true;
    // Delegate construction/window attachment must finish before disconnecting.
    QTimer::singleShot(0, this, [this] { m_scheduled = false; scan(); });
}

void MailRenderLifecycle::scan()
{
    bool attached = false;
    QList<QQuickItem *> pending;
    if (m_view) pending.append(m_view);
    while (!pending.isEmpty()) {
        auto *item = pending.takeLast();
        pending.append(item->childItems());
        if (!m_watched.contains(item)) {
            m_watched.insert(item);
            connect(item, &QQuickItem::childrenChanged, this, &MailRenderLifecycle::scheduleScan);
            connect(item, &QQuickItem::windowChanged, this, [this, item] {
                m_retained.remove(item);
                if (m_attached) { m_attached = false; emit attachedChanged(); }
                scheduleScan();
            });
            connect(item, &QObject::destroyed, this, [this, item] {
                m_watched.remove(item);
                m_retained.remove(item);
                scheduleScan();
            });
        }
        const QByteArray name(item->metaObject()->className());
        if (name != "QtWebEngineCore::RenderWidgetHostViewQtDelegateItem"
            && name != "RenderWidgetHostViewQtDelegateItem") continue;
        auto *window = item->window();
        if (!window) continue;
        if (m_retained.value(item) != window) {
            // Qt WebEngine releases its imported front-buffer texture on a mere
            // render-loop pause, even when the scene graph survives. Preserve it
            // until actual invalidation; its invalidated/destructor cleanup stays
            // connected. Scope this compatibility hook to this reader's delegate.
            if (QObject::disconnect(window, &QQuickWindow::sceneGraphAboutToStop, item, nullptr))
                m_retained.insert(item, window);
        }
        attached |= m_retained.value(item) == window;
    }
    if (m_attached != attached) {
        m_attached = attached;
        emit attachedChanged();
    }
}
