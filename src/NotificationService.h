#pragma once
#include <QString>

namespace nkmail {
class NotificationService {
public:
    // Best-effort desktop notification; a missing notification daemon is not an error.
    static void notify(const QString &summary, const QString &body);
};
}
