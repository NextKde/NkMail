#pragma once
#include <QObject>
#include <QHash>
#include <QVariantMap>
namespace nkmail {
class NotificationService final : public QObject {
    Q_OBJECT
public:
    explicit NotificationService(QObject *parent = nullptr);
    void notify(const QString &summary, const QString &body, const QVariantMap &target);
signals:
    void activated(const QVariantMap &target);
private slots:
    void actionInvoked(uint id, const QString &action);
    void notificationClosed(uint id, uint reason);
private:
    QHash<uint, QVariantMap> m_targets;
    quint64 m_generation = 0;
};
}
