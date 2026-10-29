#include "../../src/mail/backend.h"
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <thread>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 7) return 2;
    nkmail::Account a;
    a.name = QStringLiteral("测试发件人"); a.address = QStringLiteral("sender@fixture.invalid");
    a.username = QStringLiteral("fixture-user"); a.password = QStringLiteral("fixture-secret");
    a.imapHost = a.smtpHost = QString::fromLocal8Bit(argv[2]);
    a.imapPort = a.smtpPort = QString::fromLocal8Bit(argv[3]).toInt();
    a.imapStartTls = a.smtpStartTls = QString::fromLocal8Bit(argv[4]) == QStringLiteral("starttls");
    a.caFile = QString::fromLocal8Bit(argv[5]);
    const auto operation = QString::fromLocal8Bit(argv[1]);
    const auto scenario = QString::fromLocal8Bit(argv[6]);
    QJsonObject result;
    std::thread worker([&] {
        try {
            if (operation == QStringLiteral("folders")) {
                const auto folders = nkmail::MailioBackend::folders(a);
                QJsonArray array;
                for (const auto &f : folders) array.append(QJsonObject{{"id", f.id}, {"title", f.title}, {"total", f.total}});
                result = {{"folders", array}};
            } else if (operation == QStringLiteral("page")) {
                const auto p = nkmail::MailioBackend::page(a, QStringLiteral("INBOX"), 0, 1);
                QJsonArray array;
                for (const auto &s : p.messages) array.append(QJsonObject{{"uid", s.uid}, {"subject", s.subject}, {"unread", s.unread}, {"flagged", s.flagged}});
                result = {{"uidValidity", p.uidValidity}, {"messages", array}, {"hasMore", p.hasMore}, {"nextOffset", p.nextOffset}};
            } else if (operation == QStringLiteral("content")) {
                const auto c = nkmail::MailioBackend::content(a, QStringLiteral("INBOX"), scenario == QStringLiteral("stale") ? QStringLiteral("99") : QStringLiteral("42"), QStringLiteral("102"));
                QJsonArray attachments;
                for (const auto &at : c.attachments) attachments.append(QJsonObject{{"name", at.name}, {"bytes", QString::fromLatin1(at.bytes.toBase64())}, {"path", at.path}});
                result = {{"uidValidity", c.summary.uidValidity}, {"uid", c.summary.uid}, {"subject", c.summary.subject}, {"text", c.text}, {"attachments", attachments}, {"to", c.to}, {"cc", c.cc}, {"replyTo", c.replyTo}};
            } else if (operation == QStringLiteral("send")) {
                nkmail::Draft d;
                d.to = QStringLiteral("收件人 <to@fixture.invalid>"); d.cc = QStringLiteral("cc@fixture.invalid"); d.bcc = QStringLiteral("bcc@fixture.invalid");
                d.subject = QStringLiteral("中文协议测试"); d.body = QStringLiteral("你好，世界。\n.text\n");
                nkmail::Attachment attachment;
                attachment.name = QStringLiteral("../测试.txt"); attachment.mimeType = QStringLiteral("text/plain");
                attachment.bytes = QByteArray("synthetic attachment\0bytes", 26); d.attachments << attachment;
                const auto s = nkmail::MailioBackend::send(a, d);
                result = {{"state", int(s.state)}, {"message", s.message}};
            } else return;
            result["ok"] = true;
        } catch (const std::exception &e) {
            result = {{"ok", false}, {"error", QString::fromLatin1(e.what())}};
        }
    });
    worker.join();
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
    return 0;
}
