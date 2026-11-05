#pragma once
#include <QString>
#include <QHash>

namespace nkmail {
QString sanitizeMailHtml(const QString &input);
// Browser-only output: preserve layout; requires the isolated MailWebProfile/CSP.
// CID values must be application-validated raster data URLs, never sender markup.
QString sanitizeBrowserMailHtml(const QString &input, const QHash<QString, QString> &inlineImages = {});
}
