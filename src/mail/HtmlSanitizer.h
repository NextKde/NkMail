#pragma once
#include <QString>

namespace nkmail {
QString sanitizeMailHtml(const QString &input);
// Browser-only output: preserve layout; requires the isolated MailWebProfile/CSP.
QString sanitizeBrowserMailHtml(const QString &input);
}
