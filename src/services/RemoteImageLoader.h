#pragma once
#include "TaskRunner.h"
#include "MailTypes.h"
#include <QObject>
#include <QCache>
#include <QSet>
#include <atomic>
#include <memory>
namespace nkmail {
class RemoteImageLoader final : public QObject {
    Q_OBJECT
public:
    explicit RemoteImageLoader(QObject *parent = nullptr) : QObject(parent), m_cache(8 * 1024 * 1024) {}
    ~RemoteImageLoader() override { m_cancelled->store(true); m_tasks.finish(); }
    static QString key(const QString &accountId, const Summary &summary, int formatVersion = 0);
    QString cached(const QString &key, const QString &fallback) const;
    void load(const QString &key, const QString &html);
signals:
    void loaded(const QString &key, const QString &html);
private:
    QCache<QString, QString> m_cache;
    QSet<QString> m_pending;
    std::shared_ptr<std::atomic_bool> m_cancelled = std::make_shared<std::atomic_bool>(false);
    TaskRunner m_tasks;
};
}
