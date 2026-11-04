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
#include <QBuffer>
#include <QImageReader>
#include <QDebug>
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
QString RemoteImageLoader::key(const QString &accountId, const Summary &s, int formatVersion) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{accountId, s.folder, s.uidValidity, s.uid, formatVersion}).toJson(QJsonDocument::Compact));
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
    emit loadingChanged(key);
    QPointer<RemoteImageLoader> self(this);
    m_tasks.run([self, key, html, sources, cancelled = m_cancelled] {
        constexpr qint64 maxBytes = 2 * 1024 * 1024;
        constexpr int timeoutMs = 10000;
        // One manager per batch, created inside the task and never shared.
        QNetworkAccessManager manager;
        QList<QPair<QString, QString>> results;
        for (const auto &source : sources) results.append({source.first, {}});
        QEventLoop loop;
        QHash<QNetworkReply *, int> active;
        int next = 0, completed = 0;
        bool stopping = false;
        std::function<void()> schedule;
        schedule = [&] {
            while (!stopping && !cancelled->load() && active.size() < 4 && next < sources.size()) {
                const int index = next++;
                QNetworkRequest request(QUrl(sources[index].second));
                request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
                request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NkMail"));
                request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
                request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
                auto *reply = manager.get(request);
                active.insert(reply, index);
                auto *timer = new QTimer(reply); timer->setSingleShot(true);
                QObject::connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
                timer->start(timeoutMs);
                QObject::connect(reply, &QNetworkReply::readyRead, &loop, [reply] {
                    if (reply->bytesAvailable() > maxBytes) reply->abort();
                });
                QObject::connect(reply, &QNetworkReply::finished, &loop, [&, reply, index] {
                    auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().section(';', 0, 0).trimmed().toLower();
                    const auto scheme = reply->url().scheme();
                    const QByteArray data = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
                    if (reply->error() == QNetworkReply::NoError && (scheme == "http" || scheme == "https")
                        && contentType.startsWith("image/") && !data.isEmpty() && data.size() <= maxBytes) {
                        QBuffer buffer; buffer.setData(data); buffer.open(QIODevice::ReadOnly);
                        QImageReader reader(&buffer); const QSize size = reader.size();
                        if (size.isValid() && qint64(size.width()) * size.height() <= 16000000)
                            results[index].second = QStringLiteral("<img width=\"%1\" height=\"%2\" src=\"data:%3;base64,%4\">")
                                .arg(size.width()).arg(size.height()).arg(contentType, QString::fromLatin1(data.toBase64()));
                    }
                    if (results[index].second.isEmpty() && qEnvironmentVariableIsSet("NKMAIL_SYNC_DIAGNOSTICS"))
                        qInfo() << "NkMail image request failed: network=" << int(reply->error())
                            << "http=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    active.remove(reply); ++completed; reply->deleteLater();
                    if ((stopping && active.isEmpty()) || completed == sources.size()) loop.quit();
                    else schedule();
                });
            }
        };
        const auto stop = [&] {
            stopping = true;
            const auto pending = active.keys();
            for (auto *reply : pending) reply->abort();
            if (active.isEmpty()) loop.quit();
        };
        QTimer cancellation, deadline;
        QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] { if (cancelled->load()) stop(); });
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, &loop, stop);
        cancellation.start(50); deadline.start(30000);
        QTimer::singleShot(0, &loop, schedule);
        loop.exec();
        if (cancelled->load()) return;

        if (self) QMetaObject::invokeMethod(self, [self, key, html, results] {
            if (!self) return;
            self->m_pending.remove(key);
            emit self->loadingChanged(key);
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
                if (it.value().isEmpty()) {
                    QString image = match.captured();
                    if (!image.contains(QStringLiteral("data-nkmail-failed=\"1\""))) image.insert(image.size() - 1, QStringLiteral(" data-nkmail-failed=\"1\""));
                    out += image;
                } else if (match.captured().contains(QStringLiteral("data-nkmail-layout=\"1\""))
                    && it.value().startsWith(QStringLiteral("<img "))) {
                    // Preserve the sender's display dimensions/styles, rather
                    // than substituting the image's intrinsic pixel dimensions.
                    static const QRegularExpression dataSource(QStringLiteral("src=\"(data:[^\"]*)\""));
                    const auto data = dataSource.match(it.value());
                    QString image = match.captured();
                    image.remove(QStringLiteral(" data-nkmail-failed=\"1\""));
                    image.replace(srcPattern, QStringLiteral("src=\"%1\"").arg(data.captured(1)));
                    out += image;
                } else out += it.value();
                last = match.capturedEnd();
            }
            out += current.mid(last);
            self->m_cache.insert(key, new QString(out), int(out.size() * sizeof(QChar)));
            emit self->loaded(key, out);
        }, Qt::QueuedConnection);
    });
}
}
