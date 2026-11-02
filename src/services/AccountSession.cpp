#include "AccountSession.h"
#include "storage/CredentialStore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
namespace nkmail {
AccountSession::AccountSession(const QString &root, QObject *parent)
    : QObject(parent), m_root(root), m_store(new CredentialStore(this)) {
    connect(m_store, &CredentialStore::passwordReady, this, [this](const QString &key, const QString &password) {
        if (!m_waiters.contains(key)) return;
        m_passwords.insert(key, password);
        const auto waiters = m_waiters.take(key);
        for (const auto &done : waiters) done(password);
    });
    connect(m_store, &CredentialStore::error, this, [this](const QString &key, const QString &message) {
        const auto waiters = m_waiters.take(key);
        for (const auto &done : waiters) done({});
        emit error(key, message);
    });
    connect(m_store, &CredentialStore::passwordSaved, this, &AccountSession::passwordSaved);
}
QString AccountSession::key(const Account &account) const {
    const QFileInfo root(m_root);
    QString path = root.canonicalFilePath();
    if (path.isEmpty()) path = root.absoluteFilePath();
    const QByteArray scope = path.toUtf8() + '\0' + account.id.toUtf8();
    return account.id + QStringLiteral(":profile-") + QString::fromLatin1(QCryptographicHash::hash(scope, QCryptographicHash::Sha256).toHex());
}
void AccountSession::read(const QString &key) {
    if (m_passwords.contains(key)) {
        const auto password = m_passwords.value(key);
        QTimer::singleShot(0, this, [this, key, password] { emit passwordReady(key, password); });
        return;
    }
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    if (settings.contains(key + "/restoreAllowed") && !settings.value(key + "/restoreAllowed").toBool()) {
        QTimer::singleShot(0, this, [this, key] { emit error(key, tr("Enter an application password for this account.")); });
        return;
    }
    const bool pending = m_waiters.contains(key);
    m_waiters[key].append([this, key](const QString &password) { if (!password.isEmpty()) emit passwordReady(key, password); });
    if (!pending) m_store->read(key);
}
void AccountSession::setPassword(const Account &account) { if (!account.password.isEmpty()) m_passwords[key(account)] = account.password; }
void AccountSession::write(const QString &key, const QString &password) {
    m_passwords[key] = password;
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue(key + "/restoreAllowed", true);
    m_store->write(key, password);
}
void AccountSession::remove(const QString &key) {
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue(key + "/restoreAllowed", false);
    m_waiters.remove(key);
    m_store->remove(key);
}
void AccountSession::suspend(const Account &account) {
    clear();
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    settings.setValue(key(account) + "/restoreAllowed", false);
}
void AccountSession::forget(const Account &account) {
    const auto id = key(account); m_passwords.remove(id); remove(id);
}
void AccountSession::clear() { m_passwords.clear(); m_waiters.clear(); }
void AccountSession::resolve(const Account &account, std::function<void(Account)> completion) {
    const auto id = key(account);
    auto done = [account, completion = std::move(completion)](const QString &password) mutable {
        auto authenticated = account; authenticated.password = password;
        completion(authenticated);
    };
    if (!account.password.isEmpty()) { done(account.password); return; }
    if (m_passwords.contains(id)) { done(m_passwords.value(id)); return; }
    QSettings settings(QDir(m_root).filePath("credentials.ini"), QSettings::IniFormat);
    if (!settings.value(id + "/restoreAllowed", account.id == "default" && settings.value("restoreAllowed", false).toBool()).toBool()) { done({}); return; }
    const bool pending = m_waiters.contains(id);
    m_waiters[id].append(std::move(done));
    if (!pending) m_store->read(id);
}
}
