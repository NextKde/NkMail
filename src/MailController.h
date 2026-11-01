#pragma once
#include "MailTypes.h"
#include <QObject>
#include <QVariantList>
#include <QThreadPool>
#include <QTimer>
#include <memory>
#include <functional>

namespace nkmail {
class MailDatabase;
class MessageListModel;
class CredentialStore;
class MailController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* messages READ messages CONSTANT)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList drafts READ drafts NOTIFY draftsChanged)
    Q_PROPERTY(QVariantMap account READ account NOTIFY accountChanged)
    Q_PROPERTY(QVariantMap selectedMessage READ selectedMessage NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedUid READ selectedUid NOTIFY selectionChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY folderChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY statusChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY accountChanged)
    Q_PROPERTY(bool demo READ demo CONSTANT)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY statusChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(bool unreadOnly READ unreadOnly WRITE setUnreadOnly NOTIFY unreadOnlyChanged)
    Q_PROPERTY(bool notifyEnabled READ notifyEnabled WRITE setNotifyEnabled NOTIFY notifyEnabledChanged)
    Q_PROPERTY(int refreshInterval READ refreshInterval WRITE setRefreshInterval NOTIFY refreshIntervalChanged)
public:
    explicit MailController(bool demo, const QString &dataRoot, QObject *parent = nullptr);
    ~MailController() override;
    QObject *messages() const;
    QVariantList folders() const { return m_folders; }
    QVariantList drafts() const;
    QVariantMap account() const;
    QVariantMap selectedMessage() const { return m_selected; }
    QString selectedUid() const { return m_selectedUid; }
    QString currentFolder() const { return m_folder; }
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    bool busy() const { return m_busy; }
    bool sending() const { return m_sending; }
    bool configured() const;
    bool demo() const { return m_demo; }
    bool hasMore() const { return m_hasMore; }
    QString search() const { return m_search; }
    void setSearch(const QString &query);
    bool unreadOnly() const { return m_unreadOnly; }
    void setUnreadOnly(bool unreadOnly);
    bool notifyEnabled() const { return m_notifyEnabled; }
    void setNotifyEnabled(bool enabled);
    int refreshInterval() const { return m_refreshInterval; }
    void setRefreshInterval(int seconds);
    Q_INVOKABLE void configureAccount(const QVariantMap &configuration, const QString &password, bool remember);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void searchServer();
    Q_INVOKABLE void selectFolder(const QString &folder);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void openMessage(const QString &uid, const QString &uidValidity);
    Q_INVOKABLE QVariantMap newDraft(const QString &mode = QStringLiteral("new"));
    Q_INVOKABLE QVariantMap loadDraft(const QString &id);
    Q_INVOKABLE QVariantMap saveDraft(const QVariantMap &draft);
    Q_INVOKABLE void discardDraft(const QString &id);
    Q_INVOKABLE QVariantMap addAttachment(const QString &url);
    Q_INVOKABLE void saveAttachment(int index, const QString &url);
    Q_INVOKABLE void sendDraft(const QVariantMap &draft);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void disconnectAccount();
    Q_INVOKABLE void setMessageRead(int index, bool read);
    Q_INVOKABLE void toggleFlagged(int index);
    Q_INVOKABLE void deleteMessage(int index);
    Q_INVOKABLE void archiveMessage(int index);
    Q_INVOKABLE void moveMessage(int index, const QString &destination);
    Q_INVOKABLE void setMessagesRead(const QVariantList &indices, bool read);
    Q_INVOKABLE void setMessagesFlagged(const QVariantList &indices, bool flagged);
    Q_INVOKABLE void deleteMessages(const QVariantList &indices);
    Q_INVOKABLE void archiveMessages(const QVariantList &indices);
    Q_INVOKABLE void moveMessages(const QVariantList &indices, const QString &destination);
signals:
    void foldersChanged();
    void draftsChanged();
    void accountChanged();
    void selectionChanged();
    void folderChanged();
    void statusChanged();
    void searchChanged();
    void unreadOnlyChanged();
    void notifyEnabledChanged();
    void refreshIntervalChanged();
    void draftAccepted(const QString &id);
private:
    void initialize();
    void seedDemo();
    void loadCached(bool append = false);
    void fetchPage(bool append, bool quiet = false);
    void startRefreshTimer();
    void checkNewMail(const Page &page);
    void setStatus(const QString &status, const QString &error = {});
    void present(const Content &content);
    void moveMessagesTo(const QVariantList &indices, const QString &destination, bool archive);
    QString credentialKey() const;
    void run(std::function<void()> task);
    bool m_demo = false, m_busy = false, m_sending = false, m_restorePending = false, m_hasMore = false;
    bool m_serverSearching = false; // Server search is in flight, but must not block paging.
    bool m_mutating = false; // One mailbox write operation at a time.
    bool m_cachedOnly = true; // Query offsets stay local until a successful unfiltered header fetch.
    bool m_unreadOnly = false;
    bool m_notifyEnabled = true;
    int m_refreshInterval = 300; // Seconds, clamped to at least 60.
    QTimer m_refreshTimer;
    QString m_root, m_folder = QStringLiteral("INBOX"), m_status, m_error, m_search, m_selectedUid;
    QString m_uidValidity; // Selected folder generation, for validating server search.
    int m_offset = 0;
    quint64 m_generation = 0, m_selectionGeneration = 0;
    Account m_account;
    QVariantList m_folders;
    QVariantMap m_selected;
    Content m_content;
    std::unique_ptr<MailDatabase> m_database;
    MessageListModel *m_messages = nullptr;
    CredentialStore *m_credentials = nullptr;
    QThreadPool m_pool;
};
}
