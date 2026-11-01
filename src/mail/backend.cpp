#include "backend.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextDocument>
#include <QThread>
#include <QUuid>
#include <mailio/imap.hpp>
#include <mailio/smtp.hpp>
#include <algorithm>
#include <functional>
#include <limits>
#include <set>

namespace nkmail {
namespace {
constexpr qint64 MaxMessage = 25 * 1024 * 1024;
constexpr qint64 MaxAttachment = 15 * 1024 * 1024;
constexpr qint64 MaxDecoded = 25 * 1024 * 1024;
constexpr auto Timeout = std::chrono::milliseconds(15000);
using Mime = mailio::mime;
using Imap = mailio::imap;
using Message = mailio::message;

std::string utf8(const QString &s) { return s.toUtf8().toStdString(); }
QString unicode(const std::string &s, const std::string &charset = "UTF-8")
{
    QStringDecoder decoder(charset.empty() ? "UTF-8" : charset.c_str());
    const QByteArray bytes(s.data(), qsizetype(s.size()));
    return decoder.isValid() ? decoder.decode(bytes) : QString::fromUtf8(bytes);
}
QString unicode(const mailio::string_t &s) { return unicode(s.buffer, s.charset); }

void workerOnly()
{
    if (QCoreApplication::instance() &&
        QThread::currentThread() == QCoreApplication::instance()->thread())
        throw BackendError("Mail operations must run on a worker thread.");
}
void singleLine(const QString &s)
{
    for (const QChar c : s)
        if (c.unicode() < 32 || c.unicode() == 127)
            throw BackendError("Invalid mail field.");
}
void accountValid(const Account &a, bool smtp)
{
    workerOnly();
    const QString host = smtp ? a.smtpHost : a.imapHost;
    const int port = smtp ? a.smtpPort : a.imapPort;
    singleLine(host); singleLine(a.username); singleLine(a.password);
    if (host.trimmed().isEmpty() || host.contains(u'/') || port < 1 || port > 65535 ||
        a.username.isEmpty() || a.password.isEmpty())
        throw BackendError("Mail account configuration is incomplete.");
}
void folderValid(const QString &s)
{
    singleLine(s);
    if (s.isEmpty() || s.size() > 4096) throw BackendError("Invalid folder identity.");
}
unsigned long identity(const QString &s)
{
    static const QRegularExpression decimal(QStringLiteral("^[1-9][0-9]{0,9}$"));
    bool ok = false;
    const auto n = s.toULongLong(&ok);
    if (!ok || !decimal.match(s).hasMatch() || n > 0xffffffffULL)
        throw BackendError("Invalid message identity.");
    return static_cast<unsigned long>(n);
}
std::list<std::string> flagList(const QStringList &flags)
{
    static const QRegularExpression atom(QStringLiteral("^[A-Za-z0-9._\\\\-]+$"));
    if (flags.isEmpty() || flags.size() > 16) throw BackendError("Invalid message flags.");
    std::list<std::string> wire;
    for (const auto &flag : flags) {
        if (flag.isEmpty() || flag.size() > 64 || !atom.match(flag).hasMatch())
            throw BackendError("Invalid message flags.");
        wire.push_back(utf8(flag));
    }
    return wire;
}
std::string uidSet(const QStringList &uids)
{
    if (uids.size() > 500) throw BackendError("Too many messages selected.");
    std::string set;
    for (const auto &uid : uids) {
        if (!set.empty()) set += ',';
        set += std::to_string(identity(uid));
    }
    return set;
}
mailio::dialog_ssl::ssl_options_t tlsOptions(const Account &a)
{
    return {boost::asio::ssl::context::tls_client, boost::asio::ssl::verify_peer, utf8(a.caFile)};
}
void login(Imap &imap, const Account &a)
{
    imap.ssl_options(tlsOptions(a));
    imap.start_tls(a.imapStartTls);
    imap.authenticate(utf8(a.username), utf8(a.password), Imap::auth_method_t::LOGIN);
}
Imap::fetch_options_t options(bool headers)
{
    using O = Imap::fetch_options_t;
    return static_cast<O>(int(O::IS_UID) | int(O::FLAGS) | int(O::PEEK) | (headers ? int(O::HEADER_ONLY) : 0));
}
QString addressList(const mailio::mailboxes &boxes)
{
    QStringList result;
    auto add = [&](const mailio::mail_address &a) {
        QString name = unicode(a.name);
        const QString address = unicode(a.address);
        // A decoded display name is data, not an RFC 5322 phrase. Quote it so
        // commas cannot become extra recipients, escaping quoted-pair bytes.
        name.replace(u'\\', QStringLiteral("\\\\"));
        name.replace(u'"', QStringLiteral("\\\""));
        result << (name.isEmpty() ? address : u'"' + name + QStringLiteral("\" <") + address + u'>');
    };
    for (const auto &a : boxes.addresses) add(a);
    for (const auto &g : boxes.groups) for (const auto &a : g.members) add(a);
    return result.join(QStringLiteral(", "));
}
Summary summary(const Message &m, const QString &folder, const QString &validity, unsigned long uid)
{
    Summary s;
    s.folder = folder; s.uidValidity = validity; s.uid = QString::number(uid);
    const auto from = m.from();
    if (!from.addresses.empty()) {
        s.senderAddress = unicode(from.addresses.front().address);
        s.sender = unicode(from.addresses.front().name);
        if (s.sender.isEmpty()) s.sender = s.senderAddress;
    }
    s.recipients = addressList(m.recipients());
    s.subject = unicode(m.subject_raw());
    s.messageId = unicode(m.message_id());
    QStringList inReplyTo, references;
    for (const auto &id : m.in_reply_to()) inReplyTo << unicode(id);
    for (const auto &id : m.references()) references << unicode(id);
    s.inReplyTo = inReplyTo.join(QStringLiteral(" "));
    s.references = references.join(QStringLiteral(" "));
    if (!m.date_time().is_not_a_date_time())
        s.date = unicode(boost::posix_time::to_iso_extended_string(m.date_time().utc_time())) + u'Z';
    s.unread = true;
    for (const auto &f : m.flags()) {
        if (QString::fromStdString(f).compare(QStringLiteral("\\Seen"), Qt::CaseInsensitive) == 0) s.unread = false;
        if (QString::fromStdString(f).compare(QStringLiteral("\\Flagged"), Qt::CaseInsensitive) == 0) s.flagged = true;
    }
    return s;
}
QString folderTitle(const QString &raw)
{
    // RFC 3501 modified UTF-7; retain the exact wire name separately as Folder.id.
    QString result;
    for (qsizetype i = 0; i < raw.size();) {
        if (raw[i] != u'&') { result += raw[i++]; continue; }
        const auto end = raw.indexOf(u'-', i + 1);
        if (end < 0) return raw;
        if (end == i + 1) result += u'&';
        else {
            auto encoded = raw.mid(i + 1, end - i - 1).toLatin1();
            encoded.replace(',', '/');
            const auto bytes = QByteArray::fromBase64(encoded);
            if (bytes.size() % 2) return raw;
            for (qsizetype j = 0; j < bytes.size(); j += 2)
                result += QChar((quint16(quint8(bytes[j])) << 8) | quint8(bytes[j + 1]));
        }
        i = end + 1;
    }
    return result;
}
QString safeName(QString name)
{
    name.replace(u'\\', u'/');
    name = name.section(u'/', -1).trimmed();
    QString cleaned;
    for (const auto c : name)
        if (c.unicode() >= 32 && c.unicode() != 127 && !QStringLiteral(":*?\"<>|").contains(c)) cleaned += c;
    if (cleaned.isEmpty() || cleaned == QStringLiteral(".") || cleaned == QStringLiteral(".."))
        cleaned = QStringLiteral("attachment");
    return cleaned.left(200);
}
class NoResourcesDocument final : public QTextDocument {
    QVariant loadResource(int, const QUrl &) override { return {}; }
};
QString htmlText(const QString &html)
{
    // A detached document is never rendered and cannot load any URL/file resource.
    NoResourcesDocument document;
    document.setHtml(html);
    return document.toPlainText();
}
QString mimeType(Mime::content_type_t type)
{
    QString major;
    switch (type.media_type()) {
    case Mime::media_type_t::TEXT: major = QStringLiteral("text"); break;
    case Mime::media_type_t::IMAGE: major = QStringLiteral("image"); break;
    case Mime::media_type_t::AUDIO: major = QStringLiteral("audio"); break;
    case Mime::media_type_t::VIDEO: major = QStringLiteral("video"); break;
    case Mime::media_type_t::MESSAGE: major = QStringLiteral("message"); break;
    default: major = QStringLiteral("application"); break;
    }
    return major + u'/' + (type.media_subtype().empty() ? QStringLiteral("octet-stream") : unicode(type.media_subtype()));
}
struct Decoded { QStringList plain, html; QList<Attachment> attachments; qint64 bytes = 0; int nodes = 0; };
void decodePart(Mime part, Decoded &out, int depth = 0)
{
    if (depth > 24 || ++out.nodes > 500) throw BackendError("Message MIME structure exceeds safety limits.");
    const auto type = part.content_type();
    const auto data = part.content();
    out.bytes += qint64(data.size());
    if (out.bytes > MaxDecoded) throw BackendError("Message exceeds safety limits.");
    const bool attached = part.content_disposition() == Mime::content_disposition_t::ATTACHMENT || !part.name().empty();
    if (attached && type.media_type() != Mime::media_type_t::MULTIPART) {
        if (qint64(data.size()) > MaxAttachment || out.attachments.size() >= 100)
            throw BackendError("Attachments exceed safety limits.");
        Attachment a;
        a.name = safeName(unicode(part.name())); a.mimeType = mimeType(type);
        a.bytes = QByteArray(data.data(), qsizetype(data.size())); a.size = a.bytes.size();
        out.attachments << std::move(a);
        return;
    }
    if (type.media_type() == Mime::media_type_t::TEXT || type.media_type() == Mime::media_type_t::NONE) {
        const auto text = unicode(data, type.charset().buffer);
        if (QString::fromStdString(type.media_subtype()).compare(QStringLiteral("html"), Qt::CaseInsensitive) == 0)
            out.html << htmlText(text);
        else if (type.media_subtype().empty() || QString::fromStdString(type.media_subtype()).compare(QStringLiteral("plain"), Qt::CaseInsensitive) == 0)
            out.plain << text;
    }
    for (const auto &child : part.parts()) decodePart(child, out, depth + 1);
}
std::vector<mailio::mail_address> recipients(const QString &field)
{
    singleLine(field);
    if (field.trimmed().isEmpty()) return {};
    if (field.size() > 16384) throw BackendError("Recipient list exceeds safety limits.");
    Message parsed;
    parsed.line_policy(mailio::codec::line_len_policy_t::NONE);
    parsed.parse("From: sender@fixture.invalid\r\nTo: " + utf8(field) + "\r\n\r\n");
    auto list = parsed.recipients().addresses;
    for (const auto &g : parsed.recipients().groups) list.insert(list.end(), g.members.begin(), g.members.end());
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?$"));
    if (list.empty() || list.size() > 100) throw BackendError("Invalid recipient list.");
    for (auto &a : list) {
        if (!valid.match(QString::fromStdString(a.address)).hasMatch() || a.address.size() > 254)
            throw BackendError("Invalid recipient address.");
        a.name = mailio::string_t(utf8(unicode(a.name)), "UTF-8", mailio::codec::codec_t::BASE64);
    }
    return list;
}
Mime::content_type_t attachmentType(const QString &value)
{
    static const QRegularExpression valid(QStringLiteral("^([a-zA-Z0-9.+-]+)/([a-zA-Z0-9.+-]+)$"));
    const auto match = valid.match(value);
    if (!match.hasMatch()) return {Mime::media_type_t::APPLICATION, "octet-stream"};
    const auto major = match.captured(1).toLower();
    Mime::media_type_t type = Mime::media_type_t::APPLICATION;
    if (major == QStringLiteral("text")) type = Mime::media_type_t::TEXT;
    else if (major == QStringLiteral("image")) type = Mime::media_type_t::IMAGE;
    else if (major == QStringLiteral("audio")) type = Mime::media_type_t::AUDIO;
    else if (major == QStringLiteral("video")) type = Mime::media_type_t::VIDEO;
    return {type, utf8(match.captured(2))};
}
void addIds(const QString &field, const std::function<void(const std::string &)> &add)
{
    singleLine(field);
    static const QRegularExpression ids(QStringLiteral("<?([^<>\\s]+@[^<>\\s]+)>?"));
    auto matches = ids.globalMatch(field);
    int count = 0;
    while (matches.hasNext()) {
        if (++count > 100) throw BackendError("Reply references exceed safety limits.");
        add(utf8(matches.next().captured(1)));
    }
    if (!field.isEmpty() && !count) throw BackendError("Invalid reply reference.");
}
Message compose(const Account &a, const Draft &d)
{
    singleLine(a.address); singleLine(a.name); singleLine(d.subject);
    if (d.subject.size() > 16384 || d.body.toUtf8().size() + d.html.toUtf8().size() > MaxDecoded || d.attachments.size() > 100)
        throw BackendError("Draft exceeds safety limits.");
    Message m;
    m.line_policy(mailio::codec::line_len_policy_t::RECOMMENDED);
    const auto sender = recipients(a.address);
    if (sender.size() != 1) throw BackendError("Invalid sender address.");
    m.from(mailio::mail_address(mailio::string_t(utf8(a.name), "UTF-8", mailio::codec::codec_t::BASE64), sender.front().address));
    const auto to = recipients(d.to), cc = recipients(d.cc), bcc = recipients(d.bcc);
    if (to.size() + cc.size() + bcc.size() == 0 || to.size() + cc.size() + bcc.size() > 100)
        throw BackendError("At least one recipient is required (maximum 100).");
    for (const auto &r : to) m.add_recipient(r);
    for (const auto &r : cc) m.add_cc_recipient(r);
    for (const auto &r : bcc) m.add_bcc_recipient(r);
    m.subject(utf8(d.subject), mailio::codec::codec_t::BASE64);
    m.message_id(utf8(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral("@nkmail.invalid")));
    m.date_time(boost::local_time::local_date_time(boost::posix_time::second_clock::universal_time(), boost::local_time::time_zone_ptr()));
    addIds(d.inReplyTo, [&](const std::string &id) { m.add_in_reply_to(id); });
    addIds(d.references, [&](const std::string &id) { m.add_references(id); });
    Mime body;
    body.content_type(Mime::media_type_t::TEXT, "plain", "UTF-8");
    body.content_transfer_encoding(Mime::content_transfer_encoding_t::BASE_64);
    body.content(utf8(d.body));
    Mime htmlPart;
    if (!d.html.isEmpty()) {
        htmlPart.content_type(Mime::media_type_t::TEXT, "html", "UTF-8");
        htmlPart.content_transfer_encoding(Mime::content_transfer_encoding_t::BASE_64);
        htmlPart.content(utf8(d.html));
    }
    if (d.html.isEmpty()) {
        if (d.attachments.empty()) {
            m.content_type(Mime::media_type_t::TEXT, "plain", "UTF-8");
            m.content_transfer_encoding(Mime::content_transfer_encoding_t::BASE_64);
            m.content(utf8(d.body));
        } else {
            m.content_type(Mime::media_type_t::MULTIPART, "mixed");
            m.content_type().boundary(m.content_type().make_boundary());
            m.add_part(body);
        }
    } else if (d.attachments.empty()) {
        m.content_type(Mime::media_type_t::MULTIPART, "alternative");
        m.content_type().boundary(m.content_type().make_boundary());
        m.add_part(body);
        m.add_part(htmlPart);
    } else {
        Mime alternative;
        alternative.content_type(Mime::media_type_t::MULTIPART, "alternative");
        alternative.content_type().boundary(alternative.content_type().make_boundary());
        alternative.add_part(body);
        alternative.add_part(htmlPart);
        m.content_type(Mime::media_type_t::MULTIPART, "mixed");
        m.content_type().boundary(m.content_type().make_boundary());
        m.add_part(alternative);
    }
    if (!d.attachments.empty()) {
        qint64 total = d.body.toUtf8().size() + d.html.toUtf8().size();
        for (const auto &a : d.attachments) {
            QByteArray bytes = a.bytes;
            if (bytes.isEmpty() && !a.path.isEmpty()) {
                QFile file(a.path);
                if (!QFileInfo(a.path).isFile() || !file.open(QIODevice::ReadOnly) || file.size() > MaxAttachment)
                    throw BackendError("Cannot read attachment or attachment is too large.");
                bytes = file.read(MaxAttachment + 1);
                if (file.error() != QFileDevice::NoError) throw BackendError("Cannot read attachment.");
            }
            total += bytes.size();
            if (bytes.size() > MaxAttachment || total > MaxDecoded) throw BackendError("Attachments exceed safety limits.");
            Mime part;
            part.content_type(attachmentType(a.mimeType));
            part.content_disposition(Mime::content_disposition_t::ATTACHMENT);
            part.name(mailio::string_t(utf8(safeName(a.name.isEmpty() ? QFileInfo(a.path).fileName() : a.name)), "UTF-8", mailio::codec::codec_t::BASE64));
            part.content_transfer_encoding(Mime::content_transfer_encoding_t::BASE_64);
            part.content(std::string(bytes.constData(), size_t(bytes.size())));
            m.add_part(part);
        }
    }
    std::string wire;
    m.format(wire, {true, false}); // Never serialize Bcc into DATA.
    if (qint64(wire.size()) > MaxMessage) throw BackendError("Encoded message exceeds safety limits.");
    return m;
}
} // namespace

QList<Folder> MailioBackend::folders(const Account &a)
{
    try {
        accountValid(a, false);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto tree = imap.list_folders(std::string());
        const auto delimiter = unicode(imap.folder_delimiter());
        QList<Folder> result;
        int nodes = 0;
        std::function<void(const Imap::mailbox_folder_t &, const QString &, int)> visit;
        visit = [&](const auto &node, const QString &prefix, int depth) {
            if (depth > 24) throw BackendError("Folder hierarchy exceeds safety limits.");
            for (const auto &[name, child] : node.folders) {
                if (++nodes > 1000) throw BackendError("Folder list exceeds safety limits.");
                const auto id = prefix.isEmpty() ? unicode(name) : prefix + delimiter + unicode(name);
                if (child.selectable) {
                    const auto stat = imap.statistics(utf8(id));
                    result << Folder{id, folderTitle(id), int(std::min<unsigned long>(stat.messages_no, std::numeric_limits<int>::max()))};
                }
                visit(child, id, depth + 1);
            }
        };
        visit(tree, {}, 0);
        return result;
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to list folders. Check account, connection and trusted TLS certificate."); }
}
Page MailioBackend::page(const Account &a, const QString &folder, int offset, int limit)
{
    try {
        accountValid(a, false); folderValid(folder);
        if (offset < 0 || limit < 1 || limit > 100) throw BackendError("Invalid page bounds.");
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), true);
        if (!stat.uid_validity) throw BackendError("Server did not provide UIDVALIDITY.");
        Page p; p.folder = folder; p.uidValidity = QString::number(stat.uid_validity); p.nextOffset = offset;
        if (static_cast<unsigned long>(offset) >= stat.messages_no) return p;
        const auto end = stat.messages_no - static_cast<unsigned long>(offset);
        const auto begin = end >= static_cast<unsigned long>(limit) ? end - limit + 1 : 1;
        std::list<unsigned long> uids;
        // Search only this bounded sequence window, then fetch by stable UID.
        imap.search({Imap::search_condition_t(Imap::search_condition_t::SID_LIST,
                     std::list<Imap::messages_range_t>{{begin, end}})}, uids, true);
        if (uids.size() > static_cast<size_t>(limit)) throw BackendError("Server returned an invalid page.");
        imap.literal_limit(256 * 1024);
        for (auto it = uids.rbegin(); it != uids.rend(); ++it) {
            const auto messages = imap.fetch({{*it, *it}}, options(true), mailio::codec::line_len_policy_t::NONE);
            const auto found = messages.find(*it);
            if (found != messages.end()) p.messages << summary(found->second, folder, p.uidValidity, *it);
        }
        p.nextOffset = offset + int(end - begin + 1); p.hasMore = begin > 1;
        return p;
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to read message headers. Check account, connection and trusted TLS certificate."); }
}
QList<QVariantMap> MailioBackend::search(const Account &a, const QString &folder, const QString &validity,
                                        const QString &query, int limit)
{
    try {
        accountValid(a, false); folderValid(folder);
        if (query.isEmpty() || query.size() > 4096) throw BackendError("Invalid search query.");
        if (limit < 1 || limit > 200) throw BackendError("Invalid search bounds.");
        const auto expected = identity(validity);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), true);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        // A condition list is AND-combined and there is no OR combinator, so
        // search each field separately and union the UIDs.
        std::set<unsigned long> uidSet;
        int failures = 0;
        for (const auto key : {Imap::search_condition_t::SUBJECT,
                               Imap::search_condition_t::BODY,
                               Imap::search_condition_t::FROM}) {
            try {
                std::list<unsigned long> hits;
                imap.search({Imap::search_condition_t(key, utf8(query))}, hits, true);
                uidSet.insert(hits.begin(), hits.end());
            } catch (...) { ++failures; }
        }
        if (failures == 3) throw BackendError("Unable to search messages on the mail server.");
        std::list<unsigned long> uids(uidSet.begin(), uidSet.end());
        // UIDs ascend with arrival order, so walk backwards for the newest hits.
        QList<QVariantMap> result;
        int count = 0;
        imap.literal_limit(256 * 1024);
        for (auto it = uids.rbegin(); it != uids.rend() && count < limit; ++it, ++count) {
            const auto messages = imap.fetch({{*it, *it}}, options(true), mailio::codec::line_len_policy_t::NONE);
            const auto found = messages.find(*it);
            if (found != messages.end()) result << summaryMap(summary(found->second, folder, validity, *it));
        }
        return result;
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to search messages. Check account, connection and trusted TLS certificate."); }
}
Content MailioBackend::content(const Account &a, const QString &folder, const QString &validity, const QString &uid)
{
    try {
        accountValid(a, false); folderValid(folder);
        const auto expected = identity(validity), id = identity(uid);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), true);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        imap.literal_limit(MaxMessage);
        const auto messages = imap.fetch({{id, id}}, options(false), mailio::codec::line_len_policy_t::NONE);
        const auto found = messages.find(id);
        if (found == messages.end()) throw BackendError("Message no longer exists.");
        const auto &m = found->second;
        Content c; c.summary = summary(m, folder, validity, id);
        c.messageId = unicode(m.message_id());
        c.replyTo = unicode(m.reply_address().address);
        c.to = addressList(m.recipients()); c.cc = addressList(m.cc_recipients());
        Decoded decoded; decodePart(m, decoded);
        c.text = (decoded.plain.isEmpty() ? decoded.html : decoded.plain).join(QStringLiteral("\n\n"));
        c.attachments = std::move(decoded.attachments);
        c.summary.hasAttachments = !c.attachments.isEmpty();
        c.summary.preview = c.text.simplified().left(160);
        return c;
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to read message. Check account, connection and trusted TLS certificate."); }
}
SendResult MailioBackend::send(const Account &a, const Draft &d, QByteArray *raw)
{
    bool submitting = false;
    try {
        accountValid(a, true);
        const auto m = compose(a, d);
        // Format once and reuse the exact bytes for both submission and the Sent
        // copy, so the Date and Message-ID match what the recipient receives.
        std::string str;
        m.format(str, {true, false});
        if (raw) *raw = QByteArray::fromStdString(str);
        mailio::smtp smtp(utf8(a.smtpHost), a.smtpPort, Timeout);
        smtp.ssl_options(tlsOptions(a)); smtp.start_tls(a.smtpStartTls);
        smtp.source_hostname("nkmail.invalid");
        smtp.authenticate(utf8(a.username), utf8(a.password), mailio::smtp::auth_method_t::LOGIN);
        submitting = true;
        smtp.submit(m);
        return {SendResult::Accepted, QStringLiteral("SMTP server accepted the message; delivery is not confirmed.")};
    } catch (const BackendError &e) {
        return {SendResult::Rejected, QString::fromLatin1(e.what())};
    } catch (const mailio::smtp_error &e) {
        // Only known, explicit SMTP negative responses prove rejection. Parser or
        // transport failures after submission are ambiguous, never auto-retried.
        if (submitting && !e.is_rejection())
            return {SendResult::Unknown, QStringLiteral("Submission outcome is unknown. Check Sent mail before retrying manually.")};
        return {SendResult::Rejected, QStringLiteral("SMTP server rejected the operation. The draft has been preserved.")};
    } catch (...) {
        return submitting
            ? SendResult{SendResult::Unknown, QStringLiteral("Submission outcome is unknown. Check Sent mail before retrying manually.")}
            : SendResult{SendResult::Rejected, QStringLiteral("Message was not submitted. Check account, connection and trusted TLS certificate.")};
    }
}
QString MailioBackend::sentFolder(const Account &a)
{
    try {
        accountValid(a, false);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto tree = imap.list_folders(std::string());
        const auto delimiter = unicode(imap.folder_delimiter());
        QStringList ids, lasts;
        int nodes = 0;
        std::function<void(const Imap::mailbox_folder_t &, const QString &, int)> visit;
        visit = [&](const auto &node, const QString &prefix, int depth) {
            if (depth > 24 || nodes > 1000) return;
            for (const auto &[name, child] : node.folders) {
                if (++nodes > 1000) return;
                const auto id = prefix.isEmpty() ? unicode(name) : prefix + delimiter + unicode(name);
                if (child.selectable) {
                    ids << id;
                    lasts << (delimiter.isEmpty() ? id : id.section(delimiter, -1));
                }
                visit(child, id, depth + 1);
            }
        };
        visit(tree, {}, 0);
        static const QStringList candidates{
            QStringLiteral("sent"), QStringLiteral("sent items"), QStringLiteral("sent messages"),
            QStringLiteral("sent mail"), QStringLiteral("sent-mail"),
            QStringLiteral("已发送"), QStringLiteral("已发送邮件"),
            QStringLiteral("寄件備份"), QStringLiteral("寄件备份")};
        for (const auto &candidate : candidates)
            for (int i = 0; i < lasts.size(); ++i)
                if (lasts[i].compare(candidate, Qt::CaseInsensitive) == 0) return ids[i];
        for (int i = 0; i < lasts.size(); ++i)
            if (lasts[i] == QStringLiteral("Sent")) return ids[i];
        return {};
    } catch (...) { return {}; }
}
void MailioBackend::appendSent(const Account &a, const QString &folder, const QByteArray &raw)
{
    try {
        accountValid(a, false); folderValid(folder);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        imap.append(utf8(folder), std::string(raw.constData(), size_t(raw.size())), {"\\Seen"});
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to save a copy to the Sent folder. Check account, connection and trusted TLS certificate."); }
}
void MailioBackend::storeFlags(const Account &a, const QString &folder, const QString &validity,
                               const QString &uid, const QStringList &flags, bool add)
{
    try {
        accountValid(a, false); folderValid(folder);
        const auto expected = identity(validity), id = identity(uid);
        const auto wire = flagList(flags);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), false);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        imap.store_flags(id, wire, add, true);
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to update message flags. Check account, connection and trusted TLS certificate."); }
}
void MailioBackend::copyTo(const Account &a, const QString &folder, const QString &validity,
                           const QString &uid, const QString &destination)
{
    try {
        accountValid(a, false); folderValid(folder); folderValid(destination);
        const auto expected = identity(validity), id = identity(uid);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), false);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        imap.copy(id, utf8(destination), true);
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to copy message. Check account, connection and trusted TLS certificate."); }
}
void MailioBackend::expunge(const Account &a, const QString &folder, const QString &validity,
                            const QString &uid, bool *usedFullExpunge)
{
    if (usedFullExpunge) *usedFullExpunge = false;
    try {
        accountValid(a, false); folderValid(folder);
        const auto expected = identity(validity);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), false);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        if (!uid.isEmpty()) {
            const auto id = identity(uid);
            if (imap.capabilities().count("UIDPLUS")) {
                imap.expunge(id, true);
                return;
            }
            if (usedFullExpunge) *usedFullExpunge = true;
        }
        imap.expunge();
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to expunge deleted messages. Check account, connection and trusted TLS certificate."); }
}
void MailioBackend::storeFlagsMany(const Account &a, const QString &folder, const QString &validity,
                                   const QStringList &uids, const QStringList &flags, bool add)
{
    if (uids.isEmpty()) return;
    try {
        accountValid(a, false); folderValid(folder);
        const auto expected = identity(validity);
        const auto wire = flagList(flags);
        const auto set = uidSet(uids);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), false);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        imap.store_flags(set, wire, add);
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to update message flags. Check account, connection and trusted TLS certificate."); }
}
void MailioBackend::copyMany(const Account &a, const QString &folder, const QString &validity,
                             const QStringList &uids, const QString &destination)
{
    if (uids.isEmpty()) return;
    try {
        accountValid(a, false); folderValid(folder); folderValid(destination);
        const auto expected = identity(validity);
        const auto set = uidSet(uids);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), false);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        imap.copy(set, utf8(destination));
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to copy message. Check account, connection and trusted TLS certificate."); }
}
void MailioBackend::expungeMany(const Account &a, const QString &folder, const QString &validity,
                                const QStringList &uids, bool *usedFullExpunge)
{
    if (usedFullExpunge) *usedFullExpunge = false;
    if (uids.isEmpty()) return;
    try {
        accountValid(a, false); folderValid(folder);
        const auto expected = identity(validity);
        const auto set = uidSet(uids);
        Imap imap(utf8(a.imapHost), a.imapPort, Timeout); login(imap, a);
        const auto stat = imap.select(utf8(folder), false);
        if (stat.uid_validity != expected) throw BackendError("Folder identity changed. Refresh the message list.");
        if (imap.capabilities().count("UIDPLUS")) {
            imap.expunge(set);
            return;
        }
        if (usedFullExpunge) *usedFullExpunge = true;
        imap.expunge();
    } catch (const BackendError &) { throw; }
      catch (...) { throw BackendError("Unable to expunge deleted messages. Check account, connection and trusted TLS certificate."); }
}
} // namespace nkmail
