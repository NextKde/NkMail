#pragma once
#include "backend.h"
namespace nkmail {
// Injectable blocking boundary. Call on a worker; never owns UI or cache state.
class MailTransport {
public:
    virtual ~MailTransport() = default;
    virtual void verifySmtp(const Account &account) { MailioBackend::verifySmtp(account); }
    virtual InboxStats inboxStats(const Account &account) { return MailioBackend::inboxStats(account); }
    virtual QList<Folder> folders(const Account &account) { return MailioBackend::folders(account); }
    virtual Page page(const Account &account, const QString &folder,
                     int offset, int limit = 50) { return MailioBackend::page(account, folder, offset, limit); }
    virtual Content content(const Account &account, const QString &folder,
                           const QString &uidValidity, const QString &uid) { return MailioBackend::content(account, folder, uidValidity, uid); }
    virtual QList<QVariantMap> search(const Account &account, const QString &folder,
                                     const QString &uidValidity, const QString &query, int limit) { return MailioBackend::search(account, folder, uidValidity, query, limit); }
    virtual SendResult send(const Account &account, const Draft &draft, QByteArray *raw = nullptr) { return MailioBackend::send(account, draft, raw); }
    virtual QString sentFolder(const Account &account) { return MailioBackend::sentFolder(account); }
    virtual void appendSent(const Account &account, const QString &folder,
                           const QByteArray &raw) { MailioBackend::appendSent(account, folder, raw); }
    virtual void removeOrMove(const Account &account, const QString &folder,
                             const QString &uidValidity, const QStringList &uids,
                             const QString &destination = {}) { MailioBackend::removeOrMove(account, folder, uidValidity, uids, destination); }
    virtual void storeFlags(const Account &account, const QString &folder,
                           const QString &uidValidity, const QString &uid,
                           const QStringList &flags, bool add) { MailioBackend::storeFlags(account, folder, uidValidity, uid, flags, add); }
    virtual void copyTo(const Account &account, const QString &folder,
                       const QString &uidValidity, const QString &uid,
                       const QString &destination) { MailioBackend::copyTo(account, folder, uidValidity, uid, destination); }
    virtual void expunge(const Account &account, const QString &folder,
                        const QString &uidValidity, const QString &uid = {},
                        bool *usedFullExpunge = nullptr) { MailioBackend::expunge(account, folder, uidValidity, uid, usedFullExpunge); }
    virtual void storeFlagsMany(const Account &account, const QString &folder,
                               const QString &uidValidity, const QStringList &uids,
                               const QStringList &flags, bool add) { MailioBackend::storeFlagsMany(account, folder, uidValidity, uids, flags, add); }
    virtual void copyMany(const Account &account, const QString &folder,
                         const QString &uidValidity, const QStringList &uids,
                         const QString &destination) { MailioBackend::copyMany(account, folder, uidValidity, uids, destination); }
    virtual void expungeMany(const Account &account, const QString &folder,
                            const QString &uidValidity, const QStringList &uids,
                            bool *usedFullExpunge = nullptr) { MailioBackend::expungeMany(account, folder, uidValidity, uids, usedFullExpunge); }
};
}
