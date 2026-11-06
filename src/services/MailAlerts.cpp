#include "MailAlerts.h"
#include <QCryptographicHash>
#include <QDir>
#include <QSet>

namespace nkmail {
namespace {
QString accountKey(const QString &id) {
    return QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex());
}
}
MailAlerts::MailAlerts(const QString &root, QObject *parent)
    : QObject(parent), m_settings(QDir(root).filePath("notifications.ini"), QSettings::IniFormat) {}
bool MailAlerts::enabled() const { return m_settings.value("enabled", true).toBool(); }
void MailAlerts::setEnabled(bool value) { m_settings.setValue("enabled", value); m_settings.sync(); }
QString MailAlerts::identity(const Account &a) {
    return accountKey(a.id + u'\n' + a.incomingProtocol + u'\n' + a.imapHost + u'\n'
        + QString::number(a.imapPort) + u'\n' + a.username + u'\n' + a.address + u'\n' + QString::number(a.imapStartTls));
}
void MailAlerts::forget(const QString &id) { m_settings.remove("accounts/" + accountKey(id)); m_settings.sync(); }
void MailAlerts::observe(const Account &account, const Page &page, bool baselineOnly) {
    if (page.folder != QLatin1String("INBOX") || page.uidValidity.isEmpty()) return;
    const QString group = "accounts/" + accountKey(account.id) + '/' + identity(account) + '/';
    const bool initialized = m_settings.value(group + "validity").toString() == page.uidValidity;
    const bool pop = account.incomingProtocol == QLatin1String("pop3");
    const auto stored = m_settings.value(group + "seen").toStringList();
    QSet<QString> seen(stored.begin(), stored.end());
    quint64 previous = m_settings.value(group + "maxUid").toULongLong(), maximum = previous;
    if (!initialized) { seen.clear(); previous = maximum = 0; }
    int count = 0;
    Summary first;
    for (const auto &message : page.messages) {
        if (message.folder != page.folder || message.uidValidity != page.uidValidity || message.uid.isEmpty()) continue;
        bool fresh = false;
        if (pop) fresh = !seen.contains(message.uid);
        else {
            bool ok = false;
            const auto uid = message.uid.toULongLong(&ok);
            if (!ok) continue;
            maximum = qMax(maximum, uid); fresh = uid > previous;
        }
        if (fresh && message.unread) { if (first.uid.isEmpty()) first = message; ++count; }
    }
    if (pop) {
        // UIDL is opaque. Remember the complete listing, not only the newest
        // header page, so deleting newer messages never makes old mail "new".
        for (const auto &id : page.inboxIdentities) seen.insert(id);
        for (const auto &message : page.messages) seen.insert(message.uid);
        m_settings.setValue(group + "seen", QStringList(seen.begin(), seen.end()));
    } else m_settings.setValue(group + "maxUid", QVariant::fromValue(maximum));
    m_settings.setValue(group + "validity", page.uidValidity);
    m_settings.sync();
    if (!initialized || baselineOnly || !enabled() || count == 0) return;
    QVariantMap target{{"accountId", account.id}, {"folder", first.folder},
        {"uidValidity", first.uidValidity}, {"uid", first.uid}, {"identity", identity(account)}};
    const QString name = account.name.isEmpty() ? account.address : account.name;
    emit newMail(count == 1 ? tr("New mail · %1").arg(name) : tr("%1 new messages · %2").arg(count).arg(name),
                 first.subject, target);
}
}
