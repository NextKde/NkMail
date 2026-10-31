#include "NotificationService.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QStringList>
#include <QVariantMap>

namespace nkmail {
void NotificationService::notify(const QString &summary, const QString &body) {
    if (!QDBusConnection::sessionBus().isConnected()) return;
    QDBusInterface notifications(QStringLiteral("org.freedesktop.Notifications"),
                                 QStringLiteral("/org/freedesktop/Notifications"),
                                 QStringLiteral("org.freedesktop.Notifications"));
    if (!notifications.isValid()) return;
    notifications.call(QStringLiteral("Notify"),
                       QStringLiteral("NkMail"),
                       uint(0),
                       QStringLiteral("mail-message-new"),
                       summary,
                       body,
                       QStringList(),
                       QVariantMap{{QStringLiteral("desktop-entry"), QStringLiteral("nkmail")}},
                       -1);
}
}
