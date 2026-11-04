#include "auth/GoogleOAuth.h"
#include <QtTest>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <future>
#include <algorithm>
#include <chrono>
#include <vector>
#include <QDateTime>
using namespace nkmail;

class GoogleFixture : public QObject {
public:
    QTcpServer server;
    GoogleClient client;
    QMap<QString, QString> lastForm;
    QString refreshError, rotatedRefresh, scope = "openid email https://mail.google.com/";
    QString nonce;
    QJsonObject claimOverrides, headerOverrides;
    bool omitIdToken = false;
    int codeRequests = 0, refreshRequests = 0, identityRequests = 0, leakRequests = 0;
    bool verified = true, missingRefresh = false, redirect = false, stall = false;
    GoogleFixture() {
        server.listen(QHostAddress::LocalHost, 0);
        const auto base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        client.id = "fixture.apps.googleusercontent.com"; client.secret = "fixture-secret+with-plus";
        client.authorizationUrl = QUrl(base + "/authorize"); client.tokenUrl = QUrl(base + "/token");
        client.fixture = true;
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                auto buffer = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                    *buffer += socket->readAll();
                    const auto boundary = buffer->indexOf("\r\n\r\n");
                    if (boundary < 0) return;
                    const auto headers = buffer->left(boundary).split('\n');
                    int length = 0;
                    for (const auto &line : headers) if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                    if (buffer->size() < boundary + 4 + length) return;
                    disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
                    const auto target = headers.first().split(' ').value(1);
                    QJsonObject response;
                    int status = 200;
                    if (target == "/token") {
                        lastForm.clear();
                        const QUrlQuery form(QString::fromUtf8(buffer->mid(boundary + 4, length)).replace('+', ' '));
                        for (const auto &item : form.queryItems(QUrl::FullyDecoded)) lastForm.insert(item.first, item.second);
                        if (lastForm.value("grant_type") == "authorization_code") {
                            ++codeRequests;
                            response = {{"access_token", "fixture-access"}, {"token_type", "Bearer"}, {"expires_in", 3600}, {"scope", scope}};
                            if (!missingRefresh) response.insert("refresh_token", "fixture-refresh+token");
                            const auto now = QDateTime::currentSecsSinceEpoch();
                            QJsonObject claims{{"iss", "https://accounts.google.com"}, {"aud", client.id}, {"azp", client.id},
                                {"iat", now}, {"exp", now + 3600}, {"nonce", nonce}, {"sub", "fixture-subject"},
                                {"email", "reader@example.invalid"}, {"email_verified", verified},
                                {"at_hash", QString::fromLatin1(QCryptographicHash::hash("fixture-access", QCryptographicHash::Sha256)
                                    .left(16).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))}};
                            for (auto it = claimOverrides.begin(); it != claimOverrides.end(); ++it) claims.insert(it.key(), it.value());
                            QJsonObject header{{"alg", "RS256"}, {"typ", "JWT"}};
                            for (auto it = headerOverrides.begin(); it != headerOverrides.end(); ++it) header.insert(it.key(), it.value());
                            const auto encode = [](const QJsonObject &value) { return QJsonDocument(value).toJson(QJsonDocument::Compact)
                                .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals); };
                            if (!omitIdToken) response.insert("id_token", QString::fromLatin1(encode(header) + '.' + encode(claims) + ".Zml4dHVyZS1zaWduYXR1cmU"));
                        } else {
                            ++refreshRequests;
                            response = {{"access_token", "fixture-renewed"}, {"token_type", "Bearer"}, {"expires_in", 3600}, {"scope", scope}};
                            if (!rotatedRefresh.isEmpty()) response.insert("refresh_token", rotatedRefresh);
                            if (!refreshError.isEmpty()) { response = {{"error", refreshError}}; status = 400; }
                        }
                        if (stall) return;
                        if (redirect) {
                            socket->write("HTTP/1.1 302 Found\r\nLocation: " + client.tokenUrl.toString().toUtf8().replace("/token", "/leak") + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                            socket->disconnectFromHost(); return;
                        }
                    } else if (target == "/identity") {
                        ++identityRequests;
                        bool bearer = false;
                        for (const auto &line : headers) if (line.trimmed() == "Authorization: Bearer fixture-access") bearer = true;
                        response = {{"email", "reader@example.invalid"}, {"email_verified", verified && bearer}};
                    } else { ++leakRequests; status = 404; }
                    const auto data = QJsonDocument(response).toJson(QJsonDocument::Compact);
                    socket->write(QByteArray("HTTP/1.1 ") + QByteArray::number(status) + " Result\r\nContent-Type: application/json\r\nContent-Length: "
                        + QByteArray::number(data.size()) + "\r\nConnection: close\r\n\r\n" + data);
                    socket->disconnectFromHost();
                });
            }
        });
    }
};
class GoogleOAuthTests : public QObject {
    Q_OBJECT
    static QByteArray callback(const QUrl &redirect, const QByteArray &target, QString host = {}) {
        QTcpSocket socket;
        socket.connectToHost(redirect.host(), redirect.port());
        if (!socket.waitForConnected(500)) return {};
        if (host.isEmpty()) host = redirect.authority();
        socket.write("GET " + target + " HTTP/1.1\r\nHost: " + host.toUtf8() + "\r\n\r\n"); socket.flush();
        QElapsedTimer elapsed; elapsed.start();
        while (socket.state() != QAbstractSocket::UnconnectedState && elapsed.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10); QTest::qWait(1);
        }
        return socket.readAll();
    }
    static QByteArray target(const QUrlQuery &authorization, QByteArray extra = "&code=fixture-code") {
        return "/oauth2/callback?state=" + QUrl::toPercentEncoding(authorization.queryItemValue("state")) + extra;
    }
    static auto tokenAsync(const QString &opaque, QString email = "reader@example.invalid") {
        return std::async(std::launch::async, [opaque, email] {
            try { return GoogleTokens::accessToken(opaque, email); }
            catch (const GoogleOAuthError &) { return QStringLiteral("failed-safely"); }
        });
    }
private slots:
    void sharedProjectConfigurationIgnoresEndpointOverrides() {
        GoogleTokens::useFixtureClient({});
        const auto client = GoogleClient::shared();
        QVERIFY(client.valid()); QVERIFY(!client.fixture);
        QCOMPARE(client.id, "nkmail-project-fixture.apps.googleusercontent.com");
        QCOMPARE(client.tokenUrl, QUrl("https://oauth2.googleapis.com/token"));
        QCOMPARE(client.authorizationUrl, QUrl("https://accounts.google.com/o/oauth2/v2/auth"));
    }
    void missingRegistrationNeverOpensBrowser() {
        GoogleClient empty; int opened = 0;
        GoogleOAuthFlow flow(empty, [&](QUrl) { ++opened; return true; });
        QSignalSpy failed(&flow, &GoogleOAuthFlow::failed);
        flow.start("reader@example.invalid");
        QCOMPARE(failed.count(), 1); QCOMPARE(opened, 0); QVERIFY(!flow.busy());
    }
    void browserLaunchFailureAndTimeoutCloseListener() {
        GoogleFixture fixture; GoogleTokens::useFixtureClient(fixture.client);
        GoogleOAuthFlow rejected(fixture.client, [](QUrl) { return false; });
        QSignalSpy failed(&rejected, &GoogleOAuthFlow::failed); rejected.start({});
        QCOMPARE(failed.count(), 1); QVERIFY(!rejected.busy());
        auto shortClient = fixture.client; shortClient.callbackTimeoutMs = 30;
        GoogleOAuthFlow expired(shortClient, [](QUrl) { return true; });
        QSignalSpy timedOut(&expired, &GoogleOAuthFlow::failed); expired.start({});
        QTRY_COMPARE(timedOut.count(), 1); QVERIFY(!expired.busy());
    }
    void authorizationUsesPkceAndVerifiedIdentity() {
        GoogleFixture fixture; GoogleTokens::useFixtureClient(fixture.client);
        QUrl browserUrl;
        GoogleOAuthFlow flow(fixture.client, [&](QUrl url) { browserUrl = url; fixture.nonce = QUrlQuery(url).queryItemValue("nonce"); return true; });
        QSignalSpy granted(&flow, &GoogleOAuthFlow::granted);
        flow.start("reader@example.invalid");
        const QUrlQuery authorization(browserUrl);
        QCOMPARE(authorization.queryItemValue("code_challenge_method"), "S256");
        QCOMPARE(authorization.queryItemValue("access_type"), "offline");
        QCOMPARE(authorization.queryItemValue("prompt"), "consent");
        QVERIFY(authorization.queryItemValue("scope").contains("https://mail.google.com/"));
        QVERIFY(authorization.queryItemValue("state").size() >= 43);
        QVERIFY(authorization.queryItemValue("nonce").size() >= 43);
        QVERIFY(authorization.queryItemValue("nonce") != authorization.queryItemValue("state"));
        const QUrl redirect(authorization.queryItemValue("redirect_uri"));
        QCOMPARE(redirect.host(), "127.0.0.1"); QVERIFY(redirect.port() > 0);
        const QByteArray code = "fixture-code+with/reserved=&";
        const auto reply = callback(redirect, target(authorization, "&code=" + QUrl::toPercentEncoding(QString::fromLatin1(code))));
        QVERIFY(reply.startsWith("HTTP/1.1 200")); QVERIFY(!reply.contains(code));
        QTRY_COMPARE(granted.count(), 1);
        QCOMPARE(granted.at(0).at(0).toString(), "reader@example.invalid");
        const auto opaque = granted.at(0).at(1).toString();
        const auto stored = QJsonDocument::fromJson(opaque.toUtf8()).object();
        QCOMPARE(stored.value("refresh_token").toString(), "fixture-refresh+token");
        QVERIFY(!stored.contains("access_token"));
        QCOMPARE(fixture.lastForm.value("code"), QString::fromLatin1(code));
        QCOMPARE(fixture.lastForm.value("client_secret"), fixture.client.secret);
        const auto challenge = QCryptographicHash::hash(fixture.lastForm.value("code_verifier").toLatin1(), QCryptographicHash::Sha256)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        QCOMPARE(QString::fromLatin1(challenge), authorization.queryItemValue("code_challenge"));
        auto token = tokenAsync(opaque);
        QTRY_VERIFY(token.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(token.get(), "fixture-access"); QCOMPARE(fixture.refreshRequests, 0);
        QCOMPARE(fixture.identityRequests, 0); QCOMPARE(fixture.leakRequests, 0);
        QVERIFY(!flow.busy());
        QVERIFY(callback(redirect, target(authorization)).isEmpty()); QCOMPARE(fixture.codeRequests, 1);
    }
    void wrongStateHostAndDuplicateParametersCannotConsumeFlow() {
        GoogleFixture fixture; GoogleTokens::useFixtureClient(fixture.client); QUrl browser;
        GoogleOAuthFlow flow(fixture.client, [&](QUrl url) { browser = url; fixture.nonce = QUrlQuery(url).queryItemValue("nonce"); return true; });
        QSignalSpy granted(&flow, &GoogleOAuthFlow::granted); flow.start({});
        const QUrlQuery auth(browser); const QUrl redirect(auth.queryItemValue("redirect_uri"));
        QVERIFY(callback(redirect, "/oauth2/callback?state=wrong&code=fixture").startsWith("HTTP/1.1 400"));
        QVERIFY(callback(redirect, target(auth), "attacker.invalid").startsWith("HTTP/1.1 400"));
        QVERIFY(callback(redirect, target(auth, "&code=one&code=two")).startsWith("HTTP/1.1 400"));
        QVERIFY(callback(redirect, target(auth) + "&state=duplicate").startsWith("HTTP/1.1 400"));
        QVERIFY(flow.busy()); QCOMPARE(fixture.codeRequests, 0);
        QVERIFY(callback(redirect, target(auth)).startsWith("HTTP/1.1 200"));
        QTRY_COMPARE(granted.count(), 1);
    }
    void denialAndCancellationNeverExchangeCredentials() {
        GoogleFixture fixture; GoogleTokens::useFixtureClient(fixture.client); QUrl browser;
        GoogleOAuthFlow flow(fixture.client, [&](QUrl url) { browser = url; fixture.nonce = QUrlQuery(url).queryItemValue("nonce"); return true; });
        QSignalSpy failed(&flow, &GoogleOAuthFlow::failed); flow.start({});
        const QUrlQuery auth(browser); const QUrl redirect(auth.queryItemValue("redirect_uri"));
        QVERIFY(callback(redirect, target(auth, "&error=access_denied")).startsWith("HTTP/1.1 200"));
        QCOMPARE(failed.count(), 1); QCOMPARE(fixture.codeRequests, 0); QVERIFY(!flow.busy());
        flow.start({}); flow.cancel(); QVERIFY(!flow.busy());
        QCOMPARE(fixture.codeRequests, 0);
    }
    void missingPermissionAndUnverifiedIdentityFailClosed() {
        for (int which = 0; which < 3; ++which) {
            GoogleFixture fixture; GoogleTokens::useFixtureClient(fixture.client); QUrl browser;
            fixture.verified = which != 0; fixture.missingRefresh = which == 1;
            if (which == 2) fixture.scope = "openid email";
            GoogleOAuthFlow flow(fixture.client, [&](QUrl url) { browser = url; fixture.nonce = QUrlQuery(url).queryItemValue("nonce"); return true; });
            QSignalSpy failed(&flow, &GoogleOAuthFlow::failed), granted(&flow, &GoogleOAuthFlow::granted);
            flow.start({}); const QUrlQuery auth(browser);
            callback(QUrl(auth.queryItemValue("redirect_uri")), target(auth));
            QTRY_COMPARE(failed.count(), 1); QCOMPARE(granted.count(), 0);
            const auto message = failed.first().first().toString();
            if (which == 1) {
                QVERIFY(message.contains("offline access"));
                QVERIFY(!message.contains("Gmail mail access was not granted"));
                QCOMPARE(fixture.identityRequests, 0);
            } else if (which == 2) {
                QVERIFY(message.contains("Gmail mail access was not granted"));
                QVERIFY(message.contains("allow Gmail mail access"));
                QCOMPARE(fixture.identityRequests, 0);
            }
        }
    }
    void backchannelIdentityRejectsMismatchedAndExpiredClaims() {
        const QList<QJsonObject> invalid = {
            {{"iss", "https://attacker.invalid"}}, {{"aud", "other-client"}}, {{"azp", "other-client"}},
            {{"nonce", "replayed-nonce"}}, {{"exp", QDateTime::currentSecsSinceEpoch() - 1}},
            {{"iat", QDateTime::currentSecsSinceEpoch() + 600}}, {{"sub", ""}},
            {{"email_verified", false}}, {{"email_verified", "false"}}, {{"at_hash", "wrong"}},
            {{"aud", QJsonArray{"fixture.apps.googleusercontent.com", "other-client"}}}, {{"email", "bad-address"}}
        };
        for (const auto &claims : invalid) {
            GoogleFixture fixture; fixture.claimOverrides = claims;
            GoogleTokens::useFixtureClient(fixture.client); QUrl browser;
            GoogleOAuthFlow flow(fixture.client, [&](QUrl url) { browser = url; fixture.nonce = QUrlQuery(url).queryItemValue("nonce"); return true; });
            QSignalSpy failed(&flow, &GoogleOAuthFlow::failed), granted(&flow, &GoogleOAuthFlow::granted);
            flow.start({}); const QUrlQuery auth(browser);
            callback(QUrl(auth.queryItemValue("redirect_uri")), target(auth));
            QTRY_COMPARE(failed.count(), 1); QCOMPARE(granted.count(), 0); QCOMPARE(fixture.identityRequests, 0);
        }
        for (int which = 0; which < 3; ++which) {
            GoogleFixture fixture; fixture.omitIdToken = which == 0;
            if (which == 1) fixture.headerOverrides = {{"alg", "none"}};
            if (which == 2) fixture.headerOverrides = {{"jku", "https://attacker.invalid/keys"}};
            GoogleTokens::useFixtureClient(fixture.client); QUrl browser;
            GoogleOAuthFlow flow(fixture.client, [&](QUrl url) { browser = url; fixture.nonce = QUrlQuery(url).queryItemValue("nonce"); return true; });
            QSignalSpy failed(&flow, &GoogleOAuthFlow::failed), granted(&flow, &GoogleOAuthFlow::granted);
            flow.start({}); const QUrlQuery auth(browser);
            callback(QUrl(auth.queryItemValue("redirect_uri")), target(auth));
            QTRY_COMPARE(failed.count(), 1); QCOMPARE(granted.count(), 0); QCOMPARE(fixture.identityRequests, 0);
        }
    }
    void concurrentRefreshIsSingleFlightAndIdentityBound() {
        GoogleFixture fixture; GoogleTokens::useFixtureClient(fixture.client);
        const auto opaque = GoogleTokens::credential("reader@example.invalid", "fixture-refresh+token", fixture.client.id);
        GoogleTokens::prime(opaque, "expired", QDateTime::currentSecsSinceEpoch() - 1);
        std::vector<std::future<QString>> tokens;
        for (int i = 0; i < 6; ++i) tokens.push_back(tokenAsync(opaque));
        QTRY_VERIFY(std::all_of(tokens.begin(), tokens.end(), [](auto &f) { return f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }));
        for (auto &token : tokens) QCOMPARE(token.get(), "fixture-renewed");
        QCOMPARE(fixture.refreshRequests, 1);
        QCOMPARE(fixture.lastForm.value("refresh_token"), "fixture-refresh+token");
        auto wrong = tokenAsync(opaque, "another@example.invalid");
        QTRY_VERIFY(wrong.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(wrong.get(), "failed-safely"); QCOMPARE(fixture.refreshRequests, 1);
        auto differentClient = tokenAsync(GoogleTokens::credential("reader@example.invalid", "fixture-refresh", "other.apps.googleusercontent.com"));
        QTRY_VERIFY(differentClient.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(differentClient.get(), "failed-safely"); QCOMPARE(fixture.refreshRequests, 1);
    }
    void revokedRefreshNeverFallsBackOrRetries() {
        GoogleFixture fixture; fixture.refreshError = "invalid_grant"; GoogleTokens::useFixtureClient(fixture.client);
        const auto opaque = GoogleTokens::credential("reader@example.invalid", "revoked-refresh", fixture.client.id);
        for (int i = 0; i < 2; ++i) {
            auto token = tokenAsync(opaque);
            QTRY_VERIFY(token.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
            QCOMPARE(token.get(), "failed-safely");
        }
        QCOMPARE(fixture.refreshRequests, 1);
    }
    void rotationIsDeliveredForKeychainPersistenceAndForgetCancels() {
        GoogleFixture fixture; fixture.rotatedRefresh = "new-fixture-refresh"; GoogleTokens::useFixtureClient(fixture.client);
        const auto opaque = GoogleTokens::credential("reader@example.invalid", "old-fixture-refresh", fixture.client.id);
        QString replacement; QObject observer;
        GoogleTokens::observeRotation(&observer, [&](QString before, QString after) { if (before == opaque) replacement = after; });
        auto token = tokenAsync(opaque);
        QTRY_VERIFY(token.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(token.get(), "fixture-renewed"); QTRY_VERIFY(!replacement.isEmpty());
        QCOMPARE(QJsonDocument::fromJson(replacement.toUtf8()).object().value("refresh_token").toString(), fixture.rotatedRefresh);
        auto forwarded = tokenAsync(opaque);
        QTRY_VERIFY(forwarded.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(forwarded.get(), "fixture-renewed"); QCOMPARE(fixture.refreshRequests, 1);
        GoogleTokens::forget(opaque);
        auto forgotten = tokenAsync(opaque);
        QTRY_VERIFY(forgotten.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(forgotten.get(), "failed-safely");
    }
    void forgottenQueuedCredentialsCannotStartARefresh() {
        GoogleFixture fixture; GoogleTokens::useFixtureClient(fixture.client);
        const auto opaque = GoogleTokens::credential("reader@example.invalid", "queued-refresh", fixture.client.id);
        GoogleTokens::forget(opaque);
        auto queued = tokenAsync(opaque);
        QTRY_VERIFY(queued.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(queued.get(), "failed-safely");
        QCOMPARE(fixture.refreshRequests, 0);
    }
    void cancellationDoesNotReuseAnAccessTokenNearExpiry() {
        GoogleFixture fixture; fixture.stall = true; GoogleTokens::useFixtureClient(fixture.client);
        const auto opaque = GoogleTokens::credential("reader@example.invalid", "near-expiry-refresh", fixture.client.id);
        GoogleTokens::prime(opaque, "still-valid-but-cancelled", QDateTime::currentSecsSinceEpoch() + 30);
        auto active = tokenAsync(opaque); QTRY_COMPARE(fixture.refreshRequests, 1);
        GoogleTokens::forget(opaque);
        QTRY_VERIFY(active.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(active.get(), "failed-safely");
    }
    void redirectsDoNotReceiveTokensAndActiveRequestsCanBeCancelled() {
        GoogleFixture fixture; fixture.redirect = true; GoogleTokens::useFixtureClient(fixture.client);
        const auto opaque = GoogleTokens::credential("reader@example.invalid", "fixture-refresh", fixture.client.id);
        auto token = tokenAsync(opaque);
        QTRY_VERIFY(token.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(token.get(), "failed-safely"); QCOMPARE(fixture.leakRequests, 0);
        fixture.redirect = false; fixture.stall = true;
        const auto cancellable = GoogleTokens::credential("reader@example.invalid", "cancellable-refresh", fixture.client.id);
        auto active = tokenAsync(cancellable); QTRY_COMPARE(fixture.refreshRequests, 2);
        GoogleTokens::forget(cancellable);
        QTRY_VERIFY(active.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
        QCOMPARE(active.get(), "failed-safely");
    }
};
QTEST_GUILESS_MAIN(GoogleOAuthTests)
#include "tst_google_oauth.moc"
