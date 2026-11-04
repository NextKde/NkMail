#pragma once
#include "backend.h"
#include <mailio/pop3.hpp>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>

namespace nkmail {
// mailio owns connection, TLS and USER/PASS. Bound multi-line responses here,
// require stable UIDL identities, and never issue DELE (local-only removal).
class Pop3Session final : public mailio::pop3 {
public:
    explicit Pop3Session(const Account &a)
        : mailio::pop3(a.imapHost.toStdString(), a.imapPort, std::chrono::milliseconds(15000)) {
        ssl_options(mailio::dialog_ssl::ssl_options_t{boost::asio::ssl::context::tls_client,
                    boost::asio::ssl::verify_peer, a.caFile.toStdString()});
        start_tls(a.imapStartTls);
        authenticate(a.username.toUtf8().toStdString(), a.password.toUtf8().toStdString(), auth_method_t::LOGIN);
    }
    std::map<unsigned, QString> identities() {
        dlg_->send("UIDL");
        if (std::get<0>(parse_status(dlg_->receive())) != "+OK")
            throw BackendError(QT_TRANSLATE_NOOP("MailErrors", "POP server must support UIDL to identify messages safely."));
        std::map<unsigned, QString> result;
        QSet<QString> seen;
        static const QRegularExpression entry(QStringLiteral("^([1-9][0-9]{0,8}) ([\\x21-\\x7e]{1,70})$"));
        for (;;) {
            const auto line = dlg_->receive();
            if (line == ".") break;
            const auto match = entry.match(QString::fromLatin1(line.data(), line.size()));
            if (!match.hasMatch() || result.size() >= 100000)
                throw BackendError(QT_TRANSLATE_NOOP("MailErrors", "POP server returned an invalid or oversized UIDL list."));
            const auto number = match.captured(1).toUInt();
            const auto uid = match.captured(2);
            if (result.count(number) || seen.contains(uid))
                throw BackendError(QT_TRANSLATE_NOOP("MailErrors", "POP server returned duplicate message identities."));
            result.emplace(number, uid); seen.insert(uid);
        }
        return result;
    }
    QByteArray message(unsigned number, bool headers) {
        dlg_->send((headers ? "TOP " : "RETR ") + std::to_string(number) + (headers ? " 0" : ""));
        if (std::get<0>(parse_status(dlg_->receive())) != "+OK") {
            if (headers) return message(number, false); // TOP is optional; RETR never deletes.
            throw BackendError(QT_TRANSLATE_NOOP("MailErrors", "Message no longer exists."));
        }
        QByteArray raw;
        const qsizetype limit = headers ? 256 * 1024 : 25 * 1024 * 1024;
        for (;;) {
            auto line = dlg_->receive();
            if (line == ".") break;
            if (!line.empty() && line.front() == '.') line.erase(0, 1);
            if (line.size() > size_t(limit) || raw.size() + qsizetype(line.size()) + 2 > limit)
                throw BackendError(QT_TRANSLATE_NOOP("MailErrors", "POP message exceeds the download size limit."));
            raw.append(line.data(), qsizetype(line.size())); raw.append("\r\n");
        }
        return raw;
    }
};
}
