#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>
#include <QJsonObject>
#include <QPointer>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include "services/TaskRunner.h"

namespace nkmail {
struct GoogleClient {
    QString id, secret;
    QUrl authorizationUrl = QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrl tokenUrl = QUrl(QStringLiteral("https://oauth2.googleapis.com/token"));
    QUrl identityUrl = QUrl(QStringLiteral("https://openidconnect.googleapis.com/v1/userinfo"));
    int callbackTimeoutMs = 300000;
    bool fixture = false; // C++ loopback fixtures only; never loaded from user JSON.
    bool valid() const;
    static GoogleClient shared();
};
class GoogleOAuthError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
// Opaque refresh credentials use the same QtKeychain boundary as passwords.
// No token is part of accountMap/database account metadata or exposed to QML.
class GoogleTokens final {
public:
    static QString credential(const QString &email, const QString &refresh, const QString &clientId);
    static QString accessToken(const QString &credential, const QString &email);
    static void prime(const QString &credential, const QString &access, qint64 expiresAt);
    static void forget(const QString &credential);
    static void invalidateAccess(const QString &credential);
    static void observeRotation(QObject *context, std::function<void(QString, QString)> callback);
#ifdef NKMAIL_OAUTH_TESTING
    static void useFixtureClient(const GoogleClient &client);
#endif
};
class GoogleOAuthFlow final : public QObject {
    Q_OBJECT
public:
    explicit GoogleOAuthFlow(QObject *parent = nullptr);
    ~GoogleOAuthFlow() override;
    bool configured() const { return m_client.valid(); }
    bool busy() const { return m_busy; }
    void start(const QString &loginHint);
    void cancel();
#ifdef NKMAIL_OAUTH_TESTING
    GoogleOAuthFlow(GoogleClient client, std::function<bool(QUrl)> opener, QObject *parent = nullptr);
#endif
signals:
    void granted(const QString &email, const QString &credential);
    void failed(const QString &message);
private:
    void acceptConnection();
    void finishFailure(const char *message);
    void exchange(const QString &code);
    GoogleClient m_client;
    std::function<bool(QUrl)> m_openBrowser;
    QTcpServer m_listener;
    QTimer m_timeout;
    QByteArray m_state, m_verifier;
    QString m_redirect;
    bool m_busy = false;
    quint64 m_generation = 0;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    TaskRunner m_tasks;
};
}
