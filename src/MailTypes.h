#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace nkmail {
struct Account {
    QString id = QStringLiteral("default");
    QString name, address, username, password;
    QString imapHost, smtpHost;
    int imapPort = 993, smtpPort = 465;
    bool imapStartTls = false, smtpStartTls = false;
    QString caFile; // Explicit custom trust anchor; never disables verification.
};
struct Folder { QString id, title; int total = 0; };
struct Summary {
    QString folder, uidValidity, uid;
    QString sender, senderAddress, recipients, subject, date, preview;
    QString messageId, inReplyTo, references;
    bool unread = false, flagged = false, hasAttachments = false;
};
struct Attachment { QString name, mimeType, path; qint64 size = 0; QByteArray bytes; };
struct Content {
    Summary summary;
    QString text, html, messageId, replyTo, to, cc;
    QList<Attachment> attachments;
};
struct Page {
    QString folder, uidValidity;
    QList<Summary> messages;
    bool hasMore = false;
    int nextOffset = 0;
};
struct Draft {
    QString id, to, cc, bcc, subject, body, html, inReplyTo, references;
    QList<Attachment> attachments;
    QString state = QStringLiteral("draft");
    QDateTime updated;
};
struct SendResult {
    enum State { Accepted, Rejected, Unknown } state = Rejected;
    QString message;
};
QVariantMap accountMap(const Account &account);
Account accountFromMap(const QVariantMap &map);
QVariantMap summaryMap(const Summary &summary);
QVariantMap contentMap(const Content &content);
QVariantMap draftMap(const Draft &draft);
Draft draftFromMap(const QVariantMap &map);
}
