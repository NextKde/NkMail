#include "SendService.h"
#include "DraftService.h"
#include "storage/MailDatabase.h"
#include "mail/MailTransport.h"
#include "mail/HtmlSanitizer.h"
#include <QFile>
#include <QPointer>
namespace nkmail {
SendService::SendService(bool demo, MailDatabase &database, DraftService &drafts,
                         std::shared_ptr<MailTransport> transport, QObject *parent)
    : QObject(parent), m_demo(demo), m_database(&database), m_drafts(drafts), m_transport(std::move(transport)) {
    m_sendTimer.setSingleShot(true);
    connect(&m_sendTimer, &QTimer::timeout, this, [this] {
        const auto draft = m_pendingSend;
        m_pendingSend = {}; m_sendPending = false; emit sendPendingChanged();
        submitDraft(draft);
    });
}
SendService::~SendService() { m_sendTimer.stop(); m_tasks.finish(); }
void SendService::sendDraft(const Account &account, const QVariantMap &map) {
    if (m_demo) { setStatus(tr("Preview mode"), tr("Sending is disabled. Your draft is saved locally.")); m_drafts.saveDraft(map); return; }
    if (m_sending) return;
    if (m_sendPending) { setStatus(tr("A send is already pending")); return; }
    m_account = account;
    if (m_account.address.isEmpty() || m_account.password.isEmpty()) { setStatus(tr("Account needs attention"), tr("Enter an application password before sending.")); return; }
    const auto existing = m_drafts.loadDraft(map.value("id").toString());
    const auto existingState = existing.value("state").toString();
    if (existingState == "accepted" || existingState == "sending" || existingState == "unknown") {
        setStatus(tr("This draft may already have been sent"), tr("Create a new draft rather than automatically retransmitting it.")); return;
    }
    const auto saved = m_drafts.saveDraft(map); if (saved.isEmpty()) return;
    const auto draft = draftFromMap(saved);
    if (draft.state == "accepted" || draft.state == "sending" || draft.state == "unknown") { setStatus(tr("This draft may already have been sent"), tr("Create a new draft rather than automatically retransmitting it.")); return; }
    if (draft.to.trimmed().isEmpty() && draft.cc.trimmed().isEmpty() && draft.bcc.trimmed().isEmpty()) { setStatus(tr("Add a recipient")); return; }
    if (m_sendDelay <= 0) { submitDraft(draft); return; }
    // Hold the draft at "draft" until the delay expires, so a cancel leaves the
    // saved draft intact and nothing reaches the SMTP server.
    m_pendingSend = draft; m_sendPending = true;
    m_sendTimer.start(m_sendDelay * 1000);
    setStatus(tr("Sending in %1 s").arg(m_sendDelay));
    emit sendPendingChanged();
}
void SendService::submitDraft(const Draft &draft) {
    if (!m_database->markDraftState(draft.id, "sending")) { setStatus(tr("Could not persist send state"), m_database->error()); return; }
    emit draftsChanged(); m_sending = true; setStatus(tr("Submitting to SMTP server…"));
    const auto account = m_account; QPointer<SendService> self(this);
    m_tasks.run([transport = m_transport, self, account, draft] {
        SendResult result;
        QByteArray raw;
        try { result = transport->send(account, draft, &raw); }
        catch (...) { result.state = SendResult::Unknown; result.message = SendService::tr("Connection ended before the result could be confirmed."); }
        Content localCopy; bool copyComplete = true;
        if (result.state == SendResult::Accepted && account.incomingProtocol == "pop3") {
            try {
                auto &copy = localCopy; copy.summary.folder = "Sent"; copy.summary.uidValidity = "pop-local-v1";
                copy.summary.uid = draft.id; copy.summary.subject = draft.subject;
                copy.summary.sender = account.name; copy.summary.senderAddress = account.address;
                copy.summary.recipients = draft.to; copy.to = draft.to; copy.cc = draft.cc;
                copy.summary.date = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                copy.text = draft.body; copy.html = sanitizeMailHtml(draft.html);
                copy.displayHtml = sanitizeBrowserMailHtml(draft.html); copy.bodyFormatVersion = CurrentBodyFormatVersion;
                copy.attachments = draft.attachments;
                for (auto &attachment : copy.attachments) {
                    if (attachment.bytes.isEmpty() && !attachment.path.isEmpty()) {
                        QFile file(attachment.path);
                        if (!file.open(QIODevice::ReadOnly) || file.size() > 15 * 1024 * 1024) copyComplete = false;
                        else { attachment.bytes = file.read(15 * 1024 * 1024 + 1); if (attachment.bytes.size() != attachment.size) copyComplete = false; }
                    }
                    attachment.path.clear();
                }
                copy.summary.hasAttachments = !copy.attachments.isEmpty(); copy.summary.preview = draft.body.simplified().left(160);
            } catch (...) { copyComplete = false; } // SMTP acceptance must still be persisted.
        }
        if (self) QMetaObject::invokeMethod(self, [self, result, account, raw, localCopy, copyComplete, id = draft.id] {
            if (!self) return;
            const QString state = result.state == SendResult::Accepted ? "accepted" : result.state == SendResult::Rejected ? "rejected" : "unknown";
            if (!self->m_database->markDraftState(id, state)) {
                self->m_sending = false; emit self->draftsChanged();
                self->setStatus(SendService::tr("Send result could not be saved"), SendService::tr("Do not resend. Restart to recover the draft as unknown.")); return;
            }
            emit self->draftsChanged();
            if (result.state == SendResult::Accepted) {
                self->setStatus(SendService::tr("SMTP server accepted this message · delivery not confirmed")); emit self->draftAccepted(id);
                if (account.incomingProtocol == "pop3") {
                    const auto &copy = localCopy;
                    Page localPage; localPage.folder = "Sent"; localPage.uidValidity = "pop-local-v1"; localPage.messages = {copy.summary};
                    const bool saved = copyComplete && self->m_database->savePage(localPage) && self->m_database->saveContent(copy);
                    self->m_sending = false;
                    self->setStatus(saved ? SendService::tr("SMTP server accepted this message · local Sent copy saved")
                        : SendService::tr("SMTP server accepted this message · local Sent copy failed; accepted draft retained"));
                    return;
                }
                if (account.serverSavesSent) {
                    self->m_sending = false;
                    self->setStatus(SendService::tr("SMTP server accepted this message · server manages the Sent copy"));
                    return;
                }
                self->m_tasks.run([transport = self->m_transport, self, account, raw] {
                    const auto folder = transport->sentFolder(account);
                    if (folder.isEmpty()) {
                        if (self) QMetaObject::invokeMethod(self, [self] {
                            if (self) { self->m_sending = false; self->setStatus(SendService::tr("SMTP server accepted this message · no Sent folder found, no copy saved")); }
                        }, Qt::QueuedConnection);
                        return;
                    }
                    bool saved = false;
                    if (!raw.isEmpty()) {
                        try { transport->appendSent(account, folder, raw); saved = true; } catch (...) { saved = false; }
                    }
                    if (self) QMetaObject::invokeMethod(self, [self, folder, saved] {
                        if (!self) return;
                        self->m_sending = false;
                        self->setStatus(saved
                            ? SendService::tr("SMTP server accepted this message · copy saved to %1").arg(folder)
                            : SendService::tr("SMTP server accepted this message · copy could not be saved to %1").arg(folder));
                    }, Qt::QueuedConnection);
                });
            } else { self->m_sending = false; self->setStatus(result.state == SendResult::Unknown ? SendService::tr("Send result unknown · not retried") : SendService::tr("Message not accepted · draft preserved"), result.message); }
        }, Qt::QueuedConnection);
    });
}
void SendService::cancelSend() {
    if (!m_sendPending) return;
    m_sendTimer.stop();
    m_pendingSend = {}; m_sendPending = false; m_account = {};
    setStatus(tr("Send cancelled"));
    emit sendPendingChanged();
}
void SendService::clearPendingSend() {
    m_sendTimer.stop();
    const bool wasPending = !m_pendingSend.id.isEmpty();
    if (!m_sending) m_account = {};
    m_pendingSend = {}; m_sendPending = false;
    if (wasPending) emit sendPendingChanged();
}
}
