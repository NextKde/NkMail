#include "GoogleOAuth.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QTcpSocket>
#include <QThread>
#include <QUrlQuery>

namespace nkmail {
namespace {
constexpr auto NotConfigured = QT_TRANSLATE_NOOP("GoogleOAuth", "Google sign-in is unavailable in this build. Contact the application distributor.");
constexpr auto NetworkError = QT_TRANSLATE_NOOP("GoogleOAuth", "Could not contact Google securely. Check your connection and try again.");
constexpr auto InvalidReply = QT_TRANSLATE_NOOP("GoogleOAuth", "Google returned an invalid authorization response.");
constexpr auto Reauthorize = QT_TRANSLATE_NOOP("GoogleOAuth", "Google authorization has expired or was revoked. Sign in with Google again.");
constexpr auto InvalidClient = QT_TRANSLATE_NOOP("GoogleOAuth", "Google rejected this application's sign-in configuration. Contact the application distributor.");
constexpr auto AuthorizationDenied = QT_TRANSLATE_NOOP("GoogleOAuth", "Google did not authorize this account. Check the account or organization policy and try again.");
constexpr auto Cancelled = QT_TRANSLATE_NOOP("GoogleOAuth", "Google sign-in was cancelled.");
constexpr auto TimedOut = QT_TRANSLATE_NOOP("GoogleOAuth", "Google sign-in timed out. Try again.");
constexpr auto CallbackError = QT_TRANSLATE_NOOP("GoogleOAuth", "Could not open the local Google sign-in callback.");
constexpr auto BrowserError = QT_TRANSLATE_NOOP("GoogleOAuth", "Could not open the system browser for Google sign-in.");
constexpr auto MissingPermission = QT_TRANSLATE_NOOP("GoogleOAuth", "Gmail mail access was not granted. Sign in again and allow Gmail mail access on Google's consent page.");
constexpr auto MissingOfflineAccess = QT_TRANSLATE_NOOP("GoogleOAuth", "Google did not return offline access. Sign in again and complete the consent process; if this persists, contact the application distributor.");
constexpr auto IdentityError = QT_TRANSLATE_NOOP("GoogleOAuth", "Google did not return a verified email address.");
constexpr auto CallbackText = QT_TRANSLATE_NOOP("GoogleOAuth", "Authorization received. You may return to NkMail.");
constexpr auto BadCallbackText = QT_TRANSLATE_NOOP("GoogleOAuth", "This sign-in response could not be accepted.");
QString translated(const char *text) { return QCoreApplication::translate("GoogleOAuth", text); }
bool safeToken(const QString &value, int maximum = 16384) {
    if (value.isEmpty() || value.size() > maximum) return false;
    for (const auto c : value) if (c.unicode() <= 32 || c.unicode() >= 127) return false;
    return true;
}
bool validEmail(const QString &email) {
    static const QRegularExpression pattern(QStringLiteral("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$"));
    return safeToken(email, 320) && pattern.match(email).hasMatch();
}
bool endpoint(const GoogleClient &client, const QUrl &url) {
    if (client.fixture) return url.scheme() == "http" && url.host() == "127.0.0.1" && url.userInfo().isEmpty();
    return url == QUrl("https://oauth2.googleapis.com/token")
        || url == QUrl("https://openidconnect.googleapis.com/v1/userinfo");
}
QJsonObject requestJson(const GoogleClient &client, const QUrl &url, const QUrlQuery *form,
                       const QString &access, const std::shared_ptr<std::atomic_bool> &cancelled) {
    if (!endpoint(client, url)) throw GoogleOAuthError(InvalidClient);
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Cache-Control", "no-store");
    request.setTransferTimeout(20000);
    if (!access.isEmpty()) request.setRawHeader("Authorization", "Bearer " + access.toUtf8());
    QNetworkReply *reply;
    if (form) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QByteArray encoded;
        for (const auto &item : form->queryItems(QUrl::FullyDecoded)) {
            if (!encoded.isEmpty()) encoded += '&';
            encoded += QUrl::toPercentEncoding(item.first) + '=' + QUrl::toPercentEncoding(item.second);
        }
        reply = manager.post(request, encoded);
    } else reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout, cancellation;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { reply->abort(); loop.quit(); });
    QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] {
        if (cancelled && cancelled->load()) { reply->abort(); loop.quit(); }
    });
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&] {
        if (reply->bytesAvailable() > 64 * 1024) { reply->abort(); loop.quit(); }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(20000); cancellation.start(25);
    if (!reply->isFinished()) loop.exec();
    if (cancelled && cancelled->load()) throw GoogleOAuthError(Cancelled);
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->bytesAvailable() > 64 * 1024) throw GoogleOAuthError(InvalidReply);
    const auto bytes = reply->readAll();
    const auto object = QJsonDocument::fromJson(bytes).object();
    if (!client.fixture && (reply->error() != QNetworkReply::NoError || status != 200)) {
        const char *stage = !form ? "identity" : form->queryItemValue("grant_type") == "refresh_token" ? "refresh" : "exchange";
        const auto providerError = object.value("error").toString();
        const auto knownErrors = QStringList{"invalid_grant", "invalid_client", "invalid_request", "invalid_scope", "access_denied", "temporarily_unavailable", "server_error"};
        // Fixed stage, enum, status and allowlisted codes only. Never log URLs,
        // response bodies, headers, identities, authorization codes or tokens.
        qWarning().noquote() << "NkMail Google request failed:" << stage
            << "network=" << int(reply->error()) << "http=" << status
            << "provider=" << (knownErrors.contains(providerError) ? providerError : QStringLiteral("unspecified"));
    }
    if (object.value("error").toString() == "invalid_grant") throw GoogleOAuthError(Reauthorize);
    if (object.value("error").toString() == "invalid_client") throw GoogleOAuthError(InvalidClient);
    if (reply->error() != QNetworkReply::NoError || status != 200) throw GoogleOAuthError(NetworkError);
    if (object.isEmpty()) throw GoogleOAuthError(InvalidReply);
    return object;
}
qint64 expiry(const QJsonObject &tokens) {
    const auto seconds = tokens.value("expires_in").toInteger();
    if (tokens.value("token_type").toString().compare("Bearer", Qt::CaseInsensitive)
        || !safeToken(tokens.value("access_token").toString()) || seconds < 60 || seconds > 86400)
        throw GoogleOAuthError(InvalidReply);
    if (tokens.contains("scope") && !tokens.value("scope").toString().split(' ').contains("https://mail.google.com/"))
        throw GoogleOAuthError(MissingPermission);
    return QDateTime::currentSecsSinceEpoch() + seconds;
}
QByteArray randomUrlToken() {
    QByteArray bytes(32, '\0');
    for (int i = 0; i < 32; i += 4) {
        const quint32 value = QRandomGenerator::system()->generate();
        for (int j = 0; j < 4; ++j) bytes[i+j] = char(value >> (j * 8));
    }
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}
bool equalState(const QByteArray &a, const QByteArray &b) {
    if (a.size() != b.size() || a.isEmpty()) return false;
    unsigned char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i) difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}
struct TokenEntry {
    QMutex mutex;
    QString access, forwarded;
    qint64 expires = 0, retryAfter = 0;
    bool revoked = false;
    std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
};
QMutex registryMutex;
QHash<QByteArray, QSharedPointer<TokenEntry>> registry;
struct Observer { QPointer<QObject> context; std::function<void(QString, QString)> callback; };
QList<Observer> observers;
#ifdef NKMAIL_OAUTH_TESTING
GoogleClient fixtureClient;
#endif
QJsonObject parseCredential(const QString &credential) {
    if (credential.size() > 32768) throw GoogleOAuthError(Reauthorize);
    const auto object = QJsonDocument::fromJson(credential.toUtf8()).object();
    if (object.value("type").toString() != "google_oauth" || !validEmail(object.value("email").toString())
        || !safeToken(object.value("refresh_token").toString()) || !safeToken(object.value("client_id").toString(), 256))
        throw GoogleOAuthError(Reauthorize);
    return object;
}
QByteArray tokenKey(const QString &credential) {
    const auto object = parseCredential(credential);
    return QCryptographicHash::hash(object.value("client_id").toString().toUtf8() + '\0'
        + object.value("email").toString().toLower().toUtf8() + '\0'
        + object.value("refresh_token").toString().toUtf8(), QCryptographicHash::Sha256);
}
QSharedPointer<TokenEntry> tokenEntry(const QString &credential) {
    const auto key = tokenKey(credential);
    QMutexLocker guard(&registryMutex);
    if (!registry.contains(key)) registry.insert(key, QSharedPointer<TokenEntry>::create());
    return registry.value(key);
}
void notifyRotation(const QString &before, const QString &after) {
    QList<Observer> copy;
    { QMutexLocker guard(&registryMutex); copy = observers; }
    for (const auto &observer : copy) if (observer.context)
        QMetaObject::invokeMethod(observer.context, [observer, before, after] {
            if (observer.context) observer.callback(before, after);
        }, Qt::QueuedConnection);
}
}

bool GoogleClient::valid() const {
    return id.endsWith(".apps.googleusercontent.com") && safeToken(id, 256)
        && (secret.isEmpty() || safeToken(secret, 512));
}
GoogleClient GoogleClient::shared() {
#ifdef NKMAIL_OAUTH_TESTING
    { QMutexLocker guard(&registryMutex); if (fixtureClient.fixture) return fixtureClient; }
#endif
    GoogleClient client;
    QFile file(QStringLiteral(":/oauth/google-client.json"));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16384) return client;
    const auto installed = QJsonDocument::fromJson(file.readAll()).object().value("installed").toObject();
    client.id = installed.value("client_id").toString();
    client.secret = installed.value("client_secret").toString();
    // Endpoint/redirect fields in the registration JSON never override Google's URLs.
    return client;
}
QString GoogleTokens::credential(const QString &email, const QString &refresh, const QString &clientId) {
    const auto text = QString::fromUtf8(QJsonDocument(QJsonObject{{"type", "google_oauth"}, {"email", email},
        {"refresh_token", refresh}, {"client_id", clientId}}).toJson(QJsonDocument::Compact));
    parseCredential(text);
    return text;
}
void GoogleTokens::prime(const QString &credential, const QString &access, qint64 expiresAt) {
    if (!safeToken(access)) throw GoogleOAuthError(InvalidReply);
    auto fresh = QSharedPointer<TokenEntry>::create();
    fresh->access = access; fresh->expires = expiresAt;
    const auto key = tokenKey(credential);
    QMutexLocker guard(&registryMutex);
    // Publish a fresh entry rather than replacing a cancellation pointer being
    // observed by an active worker. Explicit sign-in supersedes old refreshes.
    if (auto previous = registry.value(key)) previous->cancelled->store(true);
    registry.insert(key, fresh);
}
QString GoogleTokens::accessToken(const QString &initial, const QString &email) {
    if (QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread())
        throw GoogleOAuthError(NetworkError);
    QString opaque = initial;
    for (int redirects = 0; redirects < 8; ++redirects) {
        const auto stored = parseCredential(opaque);
        const auto client = GoogleClient::shared();
        if (!client.valid() || client.id != stored.value("client_id").toString()) throw GoogleOAuthError(InvalidClient);
        if (stored.value("email").toString().compare(email, Qt::CaseInsensitive)) throw GoogleOAuthError(Reauthorize);
        auto entry = tokenEntry(opaque); QMutexLocker guard(&entry->mutex);
        if (entry->cancelled->load()) throw GoogleOAuthError(Reauthorize);
        if (!entry->forwarded.isEmpty()) { opaque = entry->forwarded; continue; }
        const auto now = QDateTime::currentSecsSinceEpoch();
        if (entry->cancelled->load() || entry->revoked) throw GoogleOAuthError(Reauthorize);
        if (!entry->access.isEmpty() && entry->expires > now + 60) return entry->access;
        if (entry->retryAfter > now) throw GoogleOAuthError(NetworkError);
        QUrlQuery form;
        form.addQueryItem("client_id", client.id);
        if (!client.secret.isEmpty()) form.addQueryItem("client_secret", client.secret);
        form.addQueryItem("refresh_token", stored.value("refresh_token").toString());
        form.addQueryItem("grant_type", "refresh_token");
        QJsonObject tokens;
        try { tokens = requestJson(client, client.tokenUrl, &form, {}, entry->cancelled); }
        catch (const GoogleOAuthError &error) {
            entry->revoked = QByteArray(error.what()) == Reauthorize;
            entry->retryAfter = now + 15;
            if (QByteArray(error.what()) == NetworkError && !entry->cancelled->load()
                && !entry->revoked && entry->expires > QDateTime::currentSecsSinceEpoch() && !entry->access.isEmpty()) return entry->access;
            throw;
        }
        entry->expires = expiry(tokens); entry->access = tokens.value("access_token").toString();
        const auto access = entry->access; const auto expiresAt = entry->expires;
        const auto refresh = tokens.value("refresh_token").toString();
        if (!refresh.isEmpty() && refresh != stored.value("refresh_token").toString()) {
            const auto updated = credential(email, refresh, client.id);
            prime(updated, access, expiresAt);
            if (entry->cancelled->load()) { forget(updated); throw GoogleOAuthError(Cancelled); }
            entry->forwarded = updated; guard.unlock();
            notifyRotation(opaque, updated);
        }
        if (entry->cancelled->load()) throw GoogleOAuthError(Cancelled);
        return access;
    }
    throw GoogleOAuthError(Reauthorize);
}
void GoogleTokens::invalidateAccess(const QString &credential) {
    QString current = credential;
    for (int i = 0; i < 8; ++i) {
        auto entry = tokenEntry(current); QMutexLocker guard(&entry->mutex);
        if (!entry->forwarded.isEmpty()) { current = entry->forwarded; continue; }
        entry->access.clear(); entry->expires = 0; return;
    }
}
void GoogleTokens::forget(const QString &credential) {
    try {
        const auto key = tokenKey(credential); QMutexLocker guard(&registryMutex);
        auto entry = registry.value(key);
        if (!entry) { entry = QSharedPointer<TokenEntry>::create(); registry.insert(key, entry); }
        entry->cancelled->store(true);
    } catch (...) { }
}
void GoogleTokens::observeRotation(QObject *context, std::function<void(QString, QString)> callback) {
    QMutexLocker guard(&registryMutex);
    for (qsizetype i = observers.size(); i-- > 0;) if (!observers[i].context) observers.removeAt(i);
    observers.append({context, std::move(callback)});
}
#ifdef NKMAIL_OAUTH_TESTING
void GoogleTokens::useFixtureClient(const GoogleClient &client) {
    QMutexLocker guard(&registryMutex); fixtureClient = client; registry.clear();
}
#endif

GoogleOAuthFlow::GoogleOAuthFlow(QObject *parent) : QObject(parent), m_client(GoogleClient::shared()),
    m_openBrowser([](const QUrl &url) { return QDesktopServices::openUrl(url); }) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finishFailure(TimedOut); });
    connect(&m_listener, &QTcpServer::newConnection, this, &GoogleOAuthFlow::acceptConnection);
}
#ifdef NKMAIL_OAUTH_TESTING
GoogleOAuthFlow::GoogleOAuthFlow(GoogleClient client, std::function<bool(QUrl)> opener, QObject *parent)
    : GoogleOAuthFlow(parent) { m_client = std::move(client); m_openBrowser = std::move(opener); }
#endif
GoogleOAuthFlow::~GoogleOAuthFlow() { cancel(); m_tasks.finish(); }
void GoogleOAuthFlow::cancel() {
    ++m_generation; m_busy = false; m_state.clear(); m_verifier.clear();
    m_timeout.stop(); m_listener.close();
    if (m_cancelled) m_cancelled->store(true);
    for (auto *socket : findChildren<QTcpSocket *>())
        if (socket->state() == QAbstractSocket::ConnectedState) socket->abort();
}
void GoogleOAuthFlow::finishFailure(const char *message) { cancel(); emit failed(translated(message)); }
void GoogleOAuthFlow::start(const QString &loginHint) {
    if (m_busy) return;
    if (!m_client.valid()) { emit failed(translated(NotConfigured)); return; }
    if (!m_listener.listen(QHostAddress::LocalHost, 0)) { emit failed(translated(CallbackError)); return; }
    m_busy = true; ++m_generation;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    m_state = randomUrlToken(); m_verifier = randomUrlToken();
    m_redirect = QStringLiteral("http://127.0.0.1:%1/oauth2/callback").arg(m_listener.serverPort());
    QUrl url = m_client.authorizationUrl;
    QUrlQuery query;
    query.addQueryItem("client_id", m_client.id); query.addQueryItem("response_type", "code");
    query.addQueryItem("redirect_uri", m_redirect); query.addQueryItem("scope", "openid email https://mail.google.com/");
    query.addQueryItem("state", QString::fromLatin1(m_state)); query.addQueryItem("access_type", "offline");
    query.addQueryItem("prompt", "consent"); query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("code_challenge", QString::fromLatin1(QCryptographicHash::hash(m_verifier, QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
    if (validEmail(loginHint)) query.addQueryItem("login_hint", loginHint);
    url.setQuery(query); m_timeout.start(m_client.fixture ? m_client.callbackTimeoutMs : 300000);
    if (!m_openBrowser(url)) finishFailure(BrowserError);
}
void GoogleOAuthFlow::acceptConnection() {
    while (auto *socket = m_listener.nextPendingConnection()) {
        if (findChildren<QTcpSocket *>().size() >= 8) { socket->abort(); socket->deleteLater(); continue; }
        socket->setParent(this);
        auto *timer = new QTimer(socket); timer->setSingleShot(true); timer->start(5000);
        connect(timer, &QTimer::timeout, socket, &QTcpSocket::abort);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
            *buffer += socket->readAll();
            if (buffer->size() > 8192) { socket->abort(); return; }
            if (!buffer->contains("\r\n\r\n")) return;
            disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
            bool accepted = false;
            QString code; QString error;
            const auto lines = buffer->split('\n');
            const auto request = lines.value(0).trimmed().split(' ');
            QString host; int hostCount = 0;
            for (const auto &line : lines) if (line.toLower().startsWith("host:")) { host = QString::fromLatin1(line.mid(5).trimmed()); ++hostCount; }
            if (request.size() == 3 && request[0] == "GET" && request[1].startsWith('/')
                && !request[1].startsWith("//") && (request[2] == "HTTP/1.1" || request[2] == "HTTP/1.0")
                && hostCount == 1 && host == QUrl(m_redirect).authority() && m_busy) {
                const QUrl target = QUrl::fromEncoded(request[1]); const QUrlQuery query(target);
                const auto states = query.allQueryItemValues("state", QUrl::FullyDecoded);
                const auto codes = query.allQueryItemValues("code", QUrl::FullyDecoded); const auto errors = query.allQueryItemValues("error", QUrl::FullyDecoded);
                accepted = target.path() == "/oauth2/callback" && target.fragment().isEmpty()
                    && states.size() == 1 && equalState(states.first().toLatin1(), m_state)
                    && ((codes.size() == 1 && errors.isEmpty() && safeToken(codes.first()))
                        || (errors.size() == 1 && codes.isEmpty() && safeToken(errors.first(), 256)));
                if (accepted) { code = codes.value(0); error = errors.value(0); }
            }
            const auto body = (QStringLiteral("<!doctype html><meta charset=utf-8><p>")
                + translated(accepted ? CallbackText : BadCallbackText).toHtmlEscaped() + QStringLiteral("</p>")).toUtf8();
            socket->write(QByteArray(accepted ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 400 Bad Request\r\n")
                + "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\n"
                  "Content-Security-Policy: default-src 'none'\r\nConnection: close\r\nContent-Length: "
                + QByteArray::number(body.size()) + "\r\n\r\n" + body);
            socket->disconnectFromHost();
            if (!accepted) return;
            m_listener.close(); m_state.clear(); // Consume once before exchanging a code.
            if (!error.isEmpty()) { finishFailure(error == "access_denied" ? Cancelled : AuthorizationDenied); return; }
            exchange(code);
        });
    }
}
void GoogleOAuthFlow::exchange(const QString &code) {
    const auto client = m_client; const auto verifier = m_verifier; const auto redirect = m_redirect;
    const auto generation = m_generation; const auto cancelled = m_cancelled;
    m_verifier.clear(); QPointer<GoogleOAuthFlow> self(this);
    m_tasks.run([self, client, verifier, redirect, code, generation, cancelled] {
        QString email, opaque, message;
        try {
            QUrlQuery form;
            form.addQueryItem("client_id", client.id);
            if (!client.secret.isEmpty()) form.addQueryItem("client_secret", client.secret);
            form.addQueryItem("code", code); form.addQueryItem("code_verifier", QString::fromLatin1(verifier));
            form.addQueryItem("redirect_uri", redirect); form.addQueryItem("grant_type", "authorization_code");
            const auto tokens = requestJson(client, client.tokenUrl, &form, {}, cancelled);
            const auto expiresAt = expiry(tokens); const auto access = tokens.value("access_token").toString();
            const auto refresh = tokens.value("refresh_token").toString();
            if (!safeToken(refresh)) throw GoogleOAuthError(MissingOfflineAccess);
            const auto identity = requestJson(client, client.identityUrl, nullptr, access, cancelled);
            email = identity.value("email").toString();
            if (!validEmail(email) || !identity.value("email_verified").toBool()) throw GoogleOAuthError(IdentityError);
            opaque = GoogleTokens::credential(email, refresh, client.id);
            if (cancelled->load()) throw GoogleOAuthError(Cancelled);
            GoogleTokens::prime(opaque, access, expiresAt);
        } catch (const GoogleOAuthError &error) { message = translated(error.what()); }
          catch (...) { message = translated(NetworkError); }
        if (self) QMetaObject::invokeMethod(self, [self, generation, email, opaque, message] {
            if (!self || self->m_generation != generation || !self->m_busy) { if (!opaque.isEmpty()) GoogleTokens::forget(opaque); return; }
            self->cancel();
            if (!message.isEmpty()) emit self->failed(message);
            else emit self->granted(email, opaque);
        }, Qt::QueuedConnection);
    });
}
}
