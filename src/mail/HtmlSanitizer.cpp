#include "HtmlSanitizer.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QStringView>

namespace nkmail {
namespace {
constexpr qsizetype MaxInputBytes = 1024 * 1024;
constexpr int MaxDepth = 64;

bool isSpace(QChar c)
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f';
}
bool isAsciiAlpha(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
}
bool isNameChar(QChar c)
{
    const ushort u = c.unicode();
    return isAsciiAlpha(c) || (u >= '0' && u <= '9') || u == '-';
}

const QSet<QString> &allowedTags()
{
    static const QSet<QString> set = {
        QStringLiteral("p"), QStringLiteral("br"), QStringLiteral("div"),
        QStringLiteral("span"), QStringLiteral("b"), QStringLiteral("strong"),
        QStringLiteral("i"), QStringLiteral("em"), QStringLiteral("u"),
        QStringLiteral("s"), QStringLiteral("strike"), QStringLiteral("blockquote"),
        QStringLiteral("ul"), QStringLiteral("ol"), QStringLiteral("li"),
        QStringLiteral("a"), QStringLiteral("h1"), QStringLiteral("h2"),
        QStringLiteral("h3"), QStringLiteral("h4"), QStringLiteral("h5"),
        QStringLiteral("h6"), QStringLiteral("pre"), QStringLiteral("code"),
        QStringLiteral("hr")};
    return set;
}
// Removed tag and everything nested inside it: remote content, scripts, forms.
const QSet<QString> &dropContainerTags()
{
    static const QSet<QString> set = {
        QStringLiteral("script"), QStringLiteral("style"), QStringLiteral("iframe"),
        QStringLiteral("object"), QStringLiteral("form"), QStringLiteral("button"),
        QStringLiteral("svg"), QStringLiteral("math"), QStringLiteral("video"),
        QStringLiteral("audio")};
    return set;
}
// Void elements in the removal list: no content to skip, just drop the tag.
const QSet<QString> &dropVoidTags()
{
    static const QSet<QString> set = {
        QStringLiteral("input"), QStringLiteral("link"), QStringLiteral("meta"),
        QStringLiteral("base"), QStringLiteral("img"), QStringLiteral("embed")};
    return set;
}
bool isVoidAllowed(const QString &name)
{
    return name == QStringLiteral("br") || name == QStringLiteral("hr");
}
// Table structure that Qt rich text cannot wrap; degraded to row text instead.
bool isTableLayoutTag(const QString &name)
{
    return name == QStringLiteral("table") || name == QStringLiteral("thead")
           || name == QStringLiteral("tbody") || name == QStringLiteral("tr")
           || name == QStringLiteral("td") || name == QStringLiteral("th");
}

QString truncateToBytes(const QString &input, qsizetype maxBytes)
{
    qint64 bytes = 0;
    for (qsizetype i = 0; i < input.size();) {
        const QChar c = input.at(i);
        const bool pair = c.isHighSurrogate() && i + 1 < input.size()
                          && input.at(i + 1).isLowSurrogate();
        int width;
        if (pair)
            width = 4;
        else {
            const ushort u = c.unicode();
            width = u < 0x80 ? 1 : (u < 0x800 ? 2 : 3);
        }
        if (bytes + width > maxBytes)
            return input.left(i);
        bytes += width;
        i += pair ? 2 : 1;
    }
    return input;
}

void appendEscaped(QString &out, QChar c)
{
    switch (c.unicode()) {
    case u'<': out += QLatin1String("&lt;"); break;
    case u'>': out += QLatin1String("&gt;"); break;
    case u'&': out += QLatin1String("&amp;"); break;
    case u'"': out += QLatin1String("&quot;"); break;
    default: out += c; break;
    }
}
void appendEscapedString(QString &out, const QString &value)
{
    for (const QChar c : value)
        appendEscaped(out, c);
}

const QHash<QString, QString> &namedEntities()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("amp"), QStringLiteral("&")},
        {QStringLiteral("lt"), QStringLiteral("<")},
        {QStringLiteral("gt"), QStringLiteral(">")},
        {QStringLiteral("quot"), QStringLiteral("\"")},
        {QStringLiteral("apos"), QStringLiteral("'")},
        {QStringLiteral("nbsp"), QString(QChar(0x00A0))},
        {QStringLiteral("copy"), QString(QChar(0x00A9))},
        {QStringLiteral("reg"), QString(QChar(0x00AE))},
        {QStringLiteral("hellip"), QString(QChar(0x2026))},
        {QStringLiteral("mdash"), QString(QChar(0x2014))},
        {QStringLiteral("ndash"), QString(QChar(0x2013))},
        {QStringLiteral("lsquo"), QString(QChar(0x2018))},
        {QStringLiteral("rsquo"), QString(QChar(0x2019))},
        {QStringLiteral("ldquo"), QString(QChar(0x201C))},
        {QStringLiteral("rdquo"), QString(QChar(0x201D))},
        {QStringLiteral("middot"), QString(QChar(0x00B7))},
        {QStringLiteral("bull"), QString(QChar(0x2022))},
        {QStringLiteral("times"), QString(QChar(0x00D7))}};
    return map;
}

bool decodeEntityAt(QStringView s, qsizetype &i, QString &decoded)
{
    const qsizetype n = s.size();
    if (i >= n || s.at(i) != u'&')
        return false;
    if (i + 1 < n && s.at(i + 1) == u'#') {
        qsizetype j = i + 2;
        const bool hex = j < n && (s.at(j) == u'x' || s.at(j) == u'X');
        if (hex)
            ++j;
        const qsizetype digits = j;
        quint32 cp = 0;
        int count = 0;
        while (j < n && count < 8) {
            const QChar c = s.at(j);
            int digit = -1;
            if (c >= u'0' && c <= u'9')
                digit = c.unicode() - u'0';
            else if (hex) {
                const QChar lower = c.toLower();
                if (lower >= u'a' && lower <= u'f')
                    digit = 10 + lower.unicode() - u'a';
            }
            if (digit < 0)
                break;
            cp = cp * (hex ? 16 : 10) + quint32(digit);
            ++j;
            ++count;
        }
        if (j > digits && j < n && s.at(j) == u';' && cp > 0 && cp <= 0x10FFFF) {
            const char32_t code = char32_t(cp);
            decoded = QString::fromUcs4(&code, 1);
            i = j + 1;
            return true;
        }
        return false;
    }
    qsizetype j = i + 1;
    const qsizetype start = j;
    while (j < n && j - start < 32) {
        const QChar lower = s.at(j).toLower();
        const bool alpha = lower >= u'a' && lower <= u'z';
        if (!alpha && !(s.at(j) >= u'0' && s.at(j) <= u'9'))
            break;
        ++j;
    }
    if (j > start && j < n && s.at(j) == u';') {
        const QString name = s.mid(start, j - start).toString().toLower();
        const auto it = namedEntities().constFind(name);
        if (it != namedEntities().constEnd()) {
            decoded = it.value();
            i = j + 1;
            return true;
        }
    }
    return false;
}

QString decodeEntities(QStringView value)
{
    QString result;
    result.reserve(value.size());
    for (qsizetype i = 0; i < value.size();) {
        if (value.at(i) == u'&') {
            QString decoded;
            if (decodeEntityAt(value, i, decoded)) {
                result += decoded;
                continue;
            }
        }
        result += value.at(i);
        ++i;
    }
    return result;
}

bool safeHref(const QString &value)
{
    if (value.isEmpty())
        return false;
    for (const QChar c : value) {
        if (c.unicode() < 0x20)
            return false;
    }
    const QString lower = value.toLower();
    return lower.startsWith(QStringLiteral("http://"))
           || lower.startsWith(QStringLiteral("https://"))
           || lower.startsWith(QStringLiteral("mailto:"));
}

struct TagParse {
    bool ok = false;
    bool closing = false;
    bool selfClosing = false;
    bool hasHref = false;
    QString name;
    QString href;
    qsizetype end = 0;
};

TagParse parseTag(QStringView s, qsizetype start)
{
    TagParse tag;
    const qsizetype n = s.size();
    qsizetype i = start + 1;
    if (i >= n)
        return tag;
    if (s.at(i) == u'!') {
        if (i + 2 < n && s.at(i + 1) == u'-' && s.at(i + 2) == u'-') {
            for (qsizetype j = i + 3; j + 2 < n; ++j) {
                if (s.at(j) == u'-' && s.at(j + 1) == u'-' && s.at(j + 2) == u'>') {
                    tag.ok = true;
                    tag.end = j + 3;
                    return tag;
                }
            }
            return tag;
        }
        const qsizetype close = s.indexOf(u'>', i + 1);
        if (close < 0)
            return tag;
        tag.ok = true;
        tag.end = close + 1;
        return tag;
    }
    if (s.at(i) == u'?') {
        const qsizetype close = s.indexOf(u'>', i + 1);
        if (close < 0)
            return tag;
        tag.ok = true;
        tag.end = close + 1;
        return tag;
    }
    if (s.at(i) == u'/') {
        tag.closing = true;
        ++i;
    }
    if (i >= n || !isAsciiAlpha(s.at(i)))
        return tag;
    const qsizetype nameStart = i;
    while (i < n && isNameChar(s.at(i)))
        ++i;
    tag.name = s.mid(nameStart, i - nameStart).toString().toLower();
    while (i < n) {
        const QChar c = s.at(i);
        if (c == u'>') {
            tag.ok = true;
            tag.end = i + 1;
            return tag;
        }
        if (c == u'/') {
            if (i + 1 < n && s.at(i + 1) == u'>') {
                tag.selfClosing = true;
                tag.ok = true;
                tag.end = i + 2;
                return tag;
            }
            ++i;
            continue;
        }
        if (isSpace(c)) {
            ++i;
            continue;
        }
        const qsizetype attrStart = i;
        while (i < n && !isSpace(s.at(i)) && s.at(i) != u'=' && s.at(i) != u'>'
               && s.at(i) != u'/')
            ++i;
        const QStringView attrName = s.mid(attrStart, i - attrStart);
        while (i < n && isSpace(s.at(i)))
            ++i;
        QString value;
        if (i < n && s.at(i) == u'=') {
            ++i;
            while (i < n && isSpace(s.at(i)))
                ++i;
            if (i < n && (s.at(i) == u'"' || s.at(i) == u'\'')) {
                const QChar quote = s.at(i);
                ++i;
                const qsizetype valueStart = i;
                while (i < n && s.at(i) != quote)
                    ++i;
                if (i >= n)
                    return tag;
                value = s.mid(valueStart, i - valueStart).toString();
                ++i;
            } else {
                const qsizetype valueStart = i;
                while (i < n && !isSpace(s.at(i)) && s.at(i) != u'>')
                    ++i;
                value = s.mid(valueStart, i - valueStart).toString();
            }
        }
        if (!tag.closing
            && attrName.compare(QStringLiteral("href"), Qt::CaseInsensitive) == 0) {
            tag.hasHref = true;
            tag.href = value;
        }
    }
    return tag;
}
}

QString sanitizeMailHtml(const QString &input)
{
    if (input.isEmpty())
        return QString();
    const QString truncated = truncateToBytes(input, MaxInputBytes);
    const QStringView s(truncated);
    const qsizetype n = s.size();

    QString out;
    out.reserve(qMin<qsizetype>(n, 1024 * 1024));
    QStringList openTags;
    QString dropName;
    int dropDepth = 0;

    qsizetype i = 0;
    while (i < n) {
        if (dropDepth > 0) {
            if (s.at(i) == u'<') {
                const TagParse tag = parseTag(s, i);
                if (tag.ok) {
                    if (!tag.name.isEmpty() && tag.name == dropName) {
                        if (tag.closing)
                            --dropDepth;
                        else if (!tag.selfClosing)
                            ++dropDepth;
                    }
                    i = tag.end;
                    continue;
                }
            }
            ++i;
            continue;
        }
        const QChar c = s.at(i);
        if (c == u'<') {
            const TagParse tag = parseTag(s, i);
            if (!tag.ok) {
                appendEscaped(out, u'<');
                ++i;
                continue;
            }
            if (!tag.name.isEmpty()) {
                if (isTableLayoutTag(tag.name)) {
                    // Qt rich text does not wrap inside tables, so wide tables
                    // overflow the reading pane. Degrade the structure to plain
                    // row text: rows become lines, cells are pipe-separated.
                    if (tag.name == QStringLiteral("tr")) {
                        if (!tag.closing)
                            out += QLatin1String("<br>");
                    } else if (tag.name == QStringLiteral("td")
                               || tag.name == QStringLiteral("th")) {
                        if (tag.closing)
                            out += QLatin1String(" | ");
                    }
                    // table, thead, tbody: drop the tag, keep the content.
                } else if (tag.closing) {
                    if (!openTags.isEmpty() && openTags.last() == tag.name) {
                        out += QLatin1String("</");
                        out += tag.name;
                        out += u'>';
                        openTags.removeLast();
                    }
                } else if (dropContainerTags().contains(tag.name)) {
                    if (!tag.selfClosing) {
                        dropName = tag.name;
                        dropDepth = 1;
                    }
                } else if (dropVoidTags().contains(tag.name)) {
                    // Dropped with no content.
                } else if (allowedTags().contains(tag.name)) {
                    if (openTags.size() < MaxDepth) {
                        if (tag.name == QStringLiteral("a")) {
                            const QString href =
                                    tag.hasHref ? decodeEntities(QStringView(tag.href)).trimmed()
                                                : QString();
                            if (safeHref(href)) {
                                out += QLatin1String("<a href=\"");
                                appendEscapedString(out, href);
                                out += QLatin1String("\">");
                                if (!tag.selfClosing)
                                    openTags.append(tag.name);
                            }
                            // Unsafe or missing href: drop the anchor, keep the text.
                        } else {
                            out += u'<';
                            out += tag.name;
                            out += u'>';
                            if (!isVoidAllowed(tag.name) && !tag.selfClosing)
                                openTags.append(tag.name);
                        }
                    }
                    // Deeper than MaxDepth: drop the tag, keep the text.
                }
                // Unknown tag: drop the tag itself, keep the text.
            }
            i = tag.end;
            continue;
        }
        if (c == u'&') {
            QString decoded;
            if (decodeEntityAt(s, i, decoded)) {
                for (const QChar d : decoded)
                    appendEscaped(out, d);
                continue;
            }
            appendEscaped(out, u'&');
            ++i;
            continue;
        }
        appendEscaped(out, c);
        ++i;
    }
    return out;
}
}
