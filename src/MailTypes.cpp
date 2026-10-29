#include "MailTypes.h"

namespace nkmail {
namespace {
QVariantList attachmentMaps(const QList<Attachment> &attachments)
{
    QVariantList result;
    for (const auto &a : attachments)
        result.append(QVariantMap{{"name", a.name}, {"mimeType", a.mimeType},
                                  {"path", a.path}, {"size", a.size}});
    return result;
}
}
QVariantMap accountMap(const Account &a)
{
    // Deliberately enumerate fields: adding a secret to Account must not serialize it.
    return {{"id", a.id}, {"name", a.name}, {"address", a.address},
            {"username", a.username}, {"imapHost", a.imapHost}, {"smtpHost", a.smtpHost},
            {"imapPort", a.imapPort}, {"smtpPort", a.smtpPort},
            {"imapStartTls", a.imapStartTls}, {"smtpStartTls", a.smtpStartTls},
            {"caFile", a.caFile}};
}
Account accountFromMap(const QVariantMap &m)
{
    Account a;
    a.id = m.value("id", a.id).toString();
    a.name = m.value("name").toString();
    a.address = m.value("address").toString();
    a.username = m.value("username").toString();
    // Input may contain a session-only password. accountMap never exposes it.
    a.password = m.value("password").toString();
    a.imapHost = m.value("imapHost").toString();
    a.smtpHost = m.value("smtpHost").toString();
    a.imapPort = m.value("imapPort", a.imapPort).toInt();
    a.smtpPort = m.value("smtpPort", a.smtpPort).toInt();
    a.imapStartTls = m.value("imapStartTls", false).toBool();
    a.smtpStartTls = m.value("smtpStartTls", false).toBool();
    a.caFile = m.value("caFile").toString();
    return a;
}
QVariantMap summaryMap(const Summary &s)
{
    return {{"folder", s.folder}, {"uidValidity", s.uidValidity}, {"uid", s.uid},
            {"sender", s.sender}, {"senderAddress", s.senderAddress},
            {"recipients", s.recipients}, {"subject", s.subject}, {"date", s.date},
            {"preview", s.preview}, {"unread", s.unread}, {"flagged", s.flagged},
            {"hasAttachments", s.hasAttachments}};
}
QVariantMap contentMap(const Content &c)
{
    QVariantMap m = summaryMap(c.summary);
    m.insert("summary", summaryMap(c.summary));
    m.insert("text", c.text);
    m.insert("messageId", c.messageId);
    m.insert("replyTo", c.replyTo);
    m.insert("to", c.to);
    m.insert("cc", c.cc);
    m.insert("attachments", attachmentMaps(c.attachments));
    return m;
}
QVariantMap draftMap(const Draft &d)
{
    return {{"id", d.id}, {"to", d.to}, {"cc", d.cc}, {"bcc", d.bcc},
            {"subject", d.subject}, {"body", d.body}, {"inReplyTo", d.inReplyTo},
            {"references", d.references}, {"attachments", attachmentMaps(d.attachments)},
            {"state", d.state}, {"updated", d.updated.toUTC().toString(Qt::ISODateWithMs)}};
}
Draft draftFromMap(const QVariantMap &m)
{
    Draft d;
    d.id = m.value("id").toString();
    d.to = m.value("to").toString();
    d.cc = m.value("cc").toString();
    d.bcc = m.value("bcc").toString();
    d.subject = m.value("subject").toString();
    d.body = m.value("body").toString();
    d.inReplyTo = m.value("inReplyTo").toString();
    d.references = m.value("references").toString();
    d.state = m.value("state", QStringLiteral("draft")).toString();
    const auto updated = m.value("updated");
    d.updated = updated.toDateTime();
    if (!d.updated.isValid())
        d.updated = QDateTime::fromString(updated.toString(), Qt::ISODateWithMs);
    for (const auto &value : m.value("attachments").toList()) {
        const auto a = value.toMap();
        d.attachments.append({a.value("name").toString(), a.value("mimeType").toString(),
                              a.value("path").toString(), a.value("size").toLongLong(), {}});
    }
    return d;
}
}
