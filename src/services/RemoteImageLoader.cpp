#include "RemoteImageLoader.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QPointer>
namespace nkmail {
namespace {
bool remoteHttpUrl(const QString &value) {
    if (value.isEmpty()) return false;
    for (const auto ch : value)
        if (ch.unicode() < 0x20) return false;
    const auto lower = value.toLower();
    return lower.startsWith(QStringLiteral("http://")) || lower.startsWith(QStringLiteral("https://"));
}
// The sanitizer HTML-escapes the placeholder URL, so the attribute read back
// from the sanitized markup must be unescaped before it can be fetched.
QString htmlAttributeText(const QString &value) {
    QString out = value;
    out.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    out.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    out.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return out;
}
}
QString RemoteImageLoader::key(const QString &accountId, const Summary &s) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{accountId, s.folder, s.uidValidity, s.uid}).toJson(QJsonDocument::Compact));
}
QString RemoteImageLoader::cached(const QString &key, const QString &fallback) const {
    if (const auto *html = m_cache.object(key)) return *html;
    return fallback;
}
void RemoteImageLoader::load(const QString &key, const QString &input) {
    if (m_pending.contains(key)) return;
    const QString html = cached(key, input);
    if (!html.contains(QStringLiteral("data-nkmail-src="))) { emit loaded(key, html); return; }
    // Collect the distinct placeholder URLs, capped so a hostile message cannot
    // turn one click into an unbounded number of requests.
    static const QRegularExpression srcPattern(QStringLiteral("data-nkmail-src=\"([^\"]*)\""));
    QList<QPair<QString, QString>> sources; // escaped attribute text, fetch URL
    QSet<QString> seen;
    auto matches = srcPattern.globalMatch(html);
    while (matches.hasNext() && sources.size() < 20) {
        const QString escaped = matches.next().captured(1);
        const QString url = htmlAttributeText(escaped).trimmed();
        if (!remoteHttpUrl(url) || seen.contains(url)) continue;
        seen.insert(url);
        sources.append({escaped, url});
    }
    if (sources.isEmpty()) return;
    m_pending.insert(key);
    QPointer<RemoteImageLoader> self(this);
    m_tasks.run([self, key, html, sources, cancelled = m_cancelled] {
        constexpr qint64 maxBytes = 2 * 1024 * 1024;
        constexpr int timeoutMs = 10000;
        // One manager per batch, created inside the task and never shared.
        QNetworkAccessManager manager;
        QList<QPair<QString, QString>> results; // escaped attribute text, replacement HTML
        for (const auto &source : sources) {
            if (cancelled->load()) return;
            const QString &escaped = source.first;
            const QUrl url(source.second);
            const bool schemeOk = url.isValid()
                && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
            QString replacement = QStringLiteral("<span>[图片加载失败]</span>");
            if (schemeOk) {
                // No Referer, no cookies: only the bare image request goes out.
                QNetworkRequest request(url);
                request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                     QNetworkRequest::NoLessSafeRedirectPolicy);
                request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NkMail"));
                request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
                request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
                QNetworkReply *reply = manager.get(request);
                QEventLoop loop;
                QTimer timer;
                timer.setSingleShot(true);
                QObject::connect(&timer, &QTimer::timeout, &loop, [&reply, &loop] {
                    reply->abort();
                    loop.quit();
                });
                // Stop buffering once a response clearly exceeds the size cap.
                QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&reply, &loop, maxBytes] {
                    if (reply->bytesAvailable() > maxBytes) {
                        reply->abort();
                        loop.quit();
                    }
                });
                QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                QTimer cancellation;
                QObject::connect(&cancellation, &QTimer::timeout, &loop, [&reply, &loop, cancelled] {
                    if (cancelled->load()) { reply->abort(); loop.quit(); }
                });
                cancellation.start(50);
                timer.start(timeoutMs);
                if (!reply->isFinished()) loop.exec();
                timer.stop();

                const QUrl finalUrl = reply->url();
                const bool finalSchemeOk = finalUrl.scheme() == QLatin1String("http")
                    || finalUrl.scheme() == QLatin1String("https");
                QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().trimmed();
                const auto semi = contentType.indexOf(QLatin1Char(';'));
                if (semi >= 0) contentType = contentType.left(semi).trimmed();
                const QByteArray data = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
                if (reply->error() == QNetworkReply::NoError && finalSchemeOk
                    && contentType.toLower().startsWith(QStringLiteral("image/"))
                    && data.size() <= maxBytes && !data.isEmpty()) {
                    replacement = QStringLiteral("<img src=\"data:%1;base64,%2\">")
                                      .arg(contentType,
                                           QString::fromLatin1(data.toBase64()));
                }
                delete reply;
            }
            results.append({escaped, replacement});
        }

        if (self) QMetaObject::invokeMethod(self, [self, key, html, results] {
            if (!self) return;
            self->m_pending.remove(key);
            const QString current = html;
            static const QRegularExpression imgPattern(
                QStringLiteral("<img\\b[^>]*\\bdata-nkmail-src=\"([^\"]*)\"[^>]*>"));
            QHash<QString, QString> replacements;
            for (const auto &result : results) replacements.insert(result.first, result.second);
            QString out;
            qsizetype last = 0;
            auto matches = imgPattern.globalMatch(current);
            while (matches.hasNext()) {
                const auto match = matches.next();
                const auto it = replacements.constFind(match.captured(1));
                if (it == replacements.constEnd()) continue;
                out += current.mid(last, match.capturedStart() - last);
                out += it.value();
                last = match.capturedEnd();
            }
            out += current.mid(last);
            self->m_cache.insert(key, new QString(out), int(out.size() * sizeof(QChar)));
            emit self->loaded(key, out);
        }, Qt::QueuedConnection);
    });
}
}
