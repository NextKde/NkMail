#pragma once
#include "TaskRunner.h"
#include "mail/MailTransport.h"
#include <QObject>
#include <QCoreApplication>
#include <QPointer>
#include <QTimer>
#include <memory>
namespace nkmail {
// Protocol scheduling only. Results return to the GUI thread; the caller checks
// its account/folder generation before committing a result to the cache/view.
class MailboxSync final : public QObject {
public:
    explicit MailboxSync(std::shared_ptr<MailTransport> transport, QObject *parent = nullptr)
        : QObject(parent), m_transport(std::move(transport)) {
        m_readIdle.setSingleShot(true);
        m_readIdle.setInterval(60000);
        connect(&m_readIdle, &QTimer::timeout, this, &MailboxSync::resetReadConnection);
    }
    void resetReadConnection() {
        m_readIdle.stop();
        m_tasks.run([transport = m_transport] { transport->resetReadConnection(); });
    }
    ~MailboxSync() override { m_tasks.finish(); }
    template<class T> using Completion = std::function<void(const T &, const QString &)>;
    void folders(Account account, Completion<QList<Folder>> done) {
        dispatch<QList<Folder>>([=, t = m_transport] { return t->folders(account); }, std::move(done));
    }
    void page(Account account, QString folder, int offset, int size, Completion<Page> done) {
        m_readIdle.start();
        dispatch<Page>([=, t = m_transport] { return t->page(account, folder, offset, size); }, std::move(done));
    }
    void content(Account account, QString folder, QString validity, QString uid, Completion<Content> done) {
        m_readIdle.start();
        dispatch<Content>([=, t = m_transport] { return t->content(account, folder, validity, uid); }, std::move(done));
    }
    void search(Account account, QString folder, QString validity, QString query, int limit, Completion<QList<QVariantMap>> done) {
        dispatch<QList<QVariantMap>>([=, t = m_transport] { return t->search(account, folder, validity, query, limit); }, std::move(done));
    }
    void stats(Account account, Completion<InboxStats> done) {
        dispatch<InboxStats>([=, t = m_transport] { return t->inboxStats(account); }, std::move(done));
    }
private:
    template<class T, class Work> void dispatch(Work work, Completion<T> done) {
        QPointer<MailboxSync> self(this);
        m_tasks.run([self, work = std::move(work), done = std::move(done)] {
            T result{}; QString error;
            try { result = work(); }
            catch (const std::exception &e) { error = QString::fromUtf8(e.what()); }
            catch (...) { error = QCoreApplication::translate("MailErrors", "Mail operation failed."); }
            if (self) QMetaObject::invokeMethod(self, [self, result, error, done] {
                if (self) done(result, error);
            }, Qt::QueuedConnection);
        });
    }
    std::shared_ptr<MailTransport> m_transport;
    TaskRunner m_tasks;
    QTimer m_readIdle;
};
}
