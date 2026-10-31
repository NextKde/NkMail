#pragma once

#include "../MailTypes.h"
#include <stdexcept>

namespace nkmail {

// Only fixed, credential-free diagnostics cross the protocol boundary.
class BackendError final : public std::runtime_error {
public:
    explicit BackendError(const char *message) : std::runtime_error(message) {}
};

// Blocking operations: invoke on a dedicated caller-owned worker thread, never
// the GUI thread. TLS verification is mandatory; no plaintext fallback/retry.
class MailioBackend final {
public:
    static QList<Folder> folders(const Account &account);
    static Page page(const Account &account, const QString &folder,
                     int offset, int limit = 50);
    static Content content(const Account &account, const QString &folder,
                           const QString &uidValidity, const QString &uid);
    static SendResult send(const Account &account, const Draft &draft);
    static void storeFlags(const Account &account, const QString &folder,
                           const QString &uidValidity, const QString &uid,
                           const QStringList &flags, bool add);
    static void copyTo(const Account &account, const QString &folder,
                       const QString &uidValidity, const QString &uid,
                       const QString &destination);
    static void expunge(const Account &account, const QString &folder,
                        const QString &uidValidity, const QString &uid = {},
                        bool *usedFullExpunge = nullptr);
};

} // namespace nkmail
