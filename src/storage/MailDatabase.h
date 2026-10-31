#pragma once
#include "MailTypes.h"
#include <QSqlDatabase>
#include <optional>

class QThread;
namespace nkmail {
// All calls, including destruction, belong to the thread that constructed this object.
// One isolated database per supplied root; passwords are never serialized.
class MailDatabase final {
public:
    MailDatabase();
    ~MailDatabase();
    MailDatabase(const MailDatabase &) = delete;
    MailDatabase &operator=(const MailDatabase &) = delete;

    bool open(const QString &root);
    void close();
    bool isOpen() const;
    QString error() const { return m_error; }
    QString errorString() const { return m_error; }
    Account account() const;
    bool saveAccount(const Account &account);
    QList<Folder> folders() const;
    bool saveFolders(const QList<Folder> &folders);
    bool invalidateUidValidity(const QString &folder, const QString &uidValidity);
    bool savePage(const Page &page);
    Page cachedPage(const QString &folder, int offset = 0, int limit = 50,
                    const QString &search = {}, bool unreadOnly = false) const;
    bool saveContent(const Content &content);
    std::optional<Content> content(const QString &folder, const QString &uidValidity,
                                   const QString &uid) const;
    bool removeMessage(const QString &folder, const QString &uidValidity, const QString &uid);
    QList<Draft> drafts() const;
    // Autosave cannot downgrade persisted sending/accepted/unknown states.
    // Read drafts() after saving if the caller needs the authoritative state.
    bool saveDraft(const Draft &draft);
    bool deleteDraft(const QString &id);
    // Compare-and-set: sending can finish; accepted/unknown can never be retried.
    // Interrupted sending is restored as unknown when opening the database.
    bool markDraftState(const QString &id, const QString &state);

private:
    bool check() const;
    bool fail(const QString &message) const;
    bool migrate();
    bool setValidity(const QString &folder, const QString &uidValidity);
    QString validity(const QString &folder) const;
    QThread *m_thread;
    QString m_connection;
    QSqlDatabase m_db;
    mutable QString m_error;
};
}
