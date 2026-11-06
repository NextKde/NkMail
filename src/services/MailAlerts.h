#pragma once
#include "MailTypes.h"
#include <QObject>
#include <QSettings>

namespace nkmail {
// Notification policy and durable account-scoped cursors. No desktop or protocol work.
class MailAlerts final : public QObject {
    Q_OBJECT
public:
    explicit MailAlerts(const QString &root, QObject *parent = nullptr);
    bool enabled() const;
    void setEnabled(bool enabled);
    void observe(const Account &account, const Page &page, bool baselineOnly = false);
    void forget(const QString &accountId);
    static QString identity(const Account &account);
signals:
    void newMail(const QString &title, const QString &body, const QVariantMap &target);
private:
    QSettings m_settings;
};
}
