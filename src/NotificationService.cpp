#include "NotificationService.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

namespace nkmail {
NotificationService::NotificationService(QObject *parent) : QObject(parent) {
    auto bus = QDBusConnection::sessionBus();
    bus.connect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
                "ActionInvoked", this, SLOT(actionInvoked(uint,QString)));
    bus.connect("org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
                "NotificationClosed", this, SLOT(notificationClosed(uint,uint)));
    auto *watcher = new QDBusServiceWatcher("org.freedesktop.Notifications", bus,
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &previous, const QString &) {
        if (!previous.isEmpty()) { ++m_generation; m_targets.clear(); }
    });
}
void NotificationService::notify(const QString &summary, const QString &body, const QVariantMap &target) {
    const auto generation = m_generation;
    const auto query = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications", "GetCapabilities");
    auto *capabilities = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(query, 5000), this);
    connect(capabilities, &QDBusPendingCallWatcher::finished, this, [this, capabilities, summary, body, target, generation] {
        QDBusPendingReply<QStringList> supported = *capabilities;
        capabilities->deleteLater();
        if (generation != m_generation) return;
        const bool markup = !supported.isError() && supported.value().contains("body-markup");
        auto message = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications", "Notify");
#ifdef NKMAIL_FLATPAK_BUILD
        const QString desktopId = QStringLiteral("org.nextkde.NkMail");
#else
        const QString desktopId = QStringLiteral("nkmail");
#endif
        message << QString("NkMail") << uint(0) << desktopId << summary << (markup ? body.toHtmlEscaped() : body)
            << QStringList{"default", tr("Open message")}
            << QVariantMap{{"desktop-entry", desktopId}} << -1;
        auto *reply = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 5000), this);
        connect(reply, &QDBusPendingCallWatcher::finished, this, [this, reply, target, generation] {
            QDBusPendingReply<uint> result = *reply;
            if (!result.isError() && generation == m_generation) m_targets.insert(result.value(), target);
            reply->deleteLater();
        });
    });
}
void NotificationService::actionInvoked(uint id, const QString &action) {
    if (action != QLatin1String("default") || !m_targets.contains(id)) return;
    emit activated(m_targets.take(id));
}
void NotificationService::notificationClosed(uint id, uint) { m_targets.remove(id); }
}
