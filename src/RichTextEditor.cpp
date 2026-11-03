#include "RichTextEditor.h"

#include <QtQml/qqml.h>
#include <QFont>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTextListFormat>

static void registerRichTextEditor()
{
    qmlRegisterType<RichTextEditor>("NextKde.Mail.Editor", 1, 0, "RichTextEditor");
}
Q_COREAPP_STARTUP_FUNCTION(registerRichTextEditor)

namespace {
QTextDocument *documentFor(QQuickTextDocument *doc) {
    return doc ? doc->textDocument() : nullptr;
}
QTextCursor selectedCursor(QTextDocument *text, int start, int end) {
    QTextCursor cursor(text);
    const int last = qMax(0, text->characterCount() - 1);
    cursor.setPosition(qBound(0, start, last));
    cursor.setPosition(qBound(0, end, last), QTextCursor::KeepAnchor);
    return cursor;
}
}

RichTextEditor::RichTextEditor(QObject *parent) : QObject(parent) {}

void RichTextEditor::setSelection(int start, int end) {
    m_selectionStart = start;
    m_selectionEnd = end;
}

void RichTextEditor::toggleBold(QQuickTextDocument *doc) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    bool autoSelected = false;
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
        autoSelected = true;
    }
    const bool active = cursor.charFormat().fontWeight() >= QFont::Bold;
    QTextCharFormat format;
    format.setFontWeight(active ? QFont::Normal : QFont::Bold);
    cursor.mergeCharFormat(format);
    if (autoSelected) cursor.clearSelection();
}

void RichTextEditor::toggleItalic(QQuickTextDocument *doc) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    bool autoSelected = false;
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
        autoSelected = true;
    }
    const bool active = cursor.charFormat().fontItalic();
    QTextCharFormat format;
    format.setFontItalic(!active);
    cursor.mergeCharFormat(format);
    if (autoSelected) cursor.clearSelection();
}

void RichTextEditor::toggleUnderline(QQuickTextDocument *doc) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    bool autoSelected = false;
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
        autoSelected = true;
    }
    const bool active = cursor.charFormat().fontUnderline();
    QTextCharFormat format;
    format.setFontUnderline(!active);
    cursor.mergeCharFormat(format);
    if (autoSelected) cursor.clearSelection();
}

void RichTextEditor::toggleBulletList(QQuickTextDocument *doc) { toggleList(doc, false); }
void RichTextEditor::toggleNumberedList(QQuickTextDocument *doc) { toggleList(doc, true); }

void RichTextEditor::toggleList(QQuickTextDocument *doc, bool numbered) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);

    const auto style = numbered ? QTextListFormat::ListDecimal : QTextListFormat::ListDisc;
    if (cursor.currentList() && cursor.currentList()->format().style() == style) {
        const QTextBlock first = text->findBlock(cursor.selectionStart());
        const QTextBlock last = text->findBlock(qMax(cursor.selectionStart(), cursor.selectionEnd() - 1));
        cursor.beginEditBlock();
        for (QTextBlock block = first; block.isValid(); block = block.next()) {
            QTextCursor blockCursor(block);
            QTextBlockFormat format = block.blockFormat();
            format.setObjectIndex(-1);
            format.setIndent(0);
            blockCursor.setBlockFormat(format);
            if (block == last) break;
        }
        cursor.endEditBlock();
        cursor.clearSelection();
        return;
    }

    QTextListFormat listFormat;
    listFormat.setStyle(style);
    listFormat.setIndent(1);
    cursor.createList(listFormat);
    cursor.clearSelection();
}

void RichTextEditor::setAlignment(QQuickTextDocument *doc, int alignment) {
    auto *text = documentFor(doc);
    if (!text) return;
    if (alignment != Qt::AlignLeft && alignment != Qt::AlignHCenter && alignment != Qt::AlignRight) return;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    QTextBlockFormat format;
    format.setAlignment(Qt::Alignment(alignment));
    cursor.mergeBlockFormat(format);
}

void RichTextEditor::changeIndent(QQuickTextDocument *doc, int delta) {
    auto *text = documentFor(doc);
    if (!text || !delta) return;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    const QTextBlock first = text->findBlock(cursor.selectionStart());
    const QTextBlock last = text->findBlock(qMax(cursor.selectionStart(), cursor.selectionEnd() - 1));
    cursor.beginEditBlock();
    for (QTextBlock block = first; block.isValid(); block = block.next()) {
        QTextCursor item(block);
        auto format = block.blockFormat();
        // Block-local indentation preserves neighbouring list items and numbering.
        format.setIndent(qBound(0, format.indent() + (delta > 0 ? 1 : -1), 12));
        item.setBlockFormat(format);
        if (block == last) break;
    }
    cursor.endEditBlock();
}

QVariantMap RichTextEditor::blockState(QQuickTextDocument *doc) const {
    auto *text = documentFor(doc);
    if (!text) return {};
    const auto cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    const auto *list = cursor.currentList();
    return {{"alignment", int(cursor.blockFormat().alignment())},
            {"bullets", list && list->format().style() == QTextListFormat::ListDisc},
            {"numbered", list && list->format().style() == QTextListFormat::ListDecimal}};
}

int RichTextEditor::insertText(QQuickTextDocument *doc, const QString &value) {
    auto *text = documentFor(doc);
    if (!text) return 0;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    cursor.beginEditBlock();
    cursor.insertText(value);
    cursor.endEditBlock();
    return cursor.position();
}

void RichTextEditor::insertLink(QQuickTextDocument *doc, const QString &url) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor = selectedCursor(text, m_selectionStart, m_selectionEnd);
    if (!cursor.hasSelection()) return;
    QTextCharFormat format;
    format.setAnchor(true);
    format.setAnchorHref(url);
    cursor.mergeCharFormat(format);
}

QString RichTextEditor::toHtml(QQuickTextDocument *doc) const {
    QTextDocument *text = documentFor(doc);
    return text ? text->toHtml() : QString();
}

QString RichTextEditor::toPlainText(QQuickTextDocument *doc) const {
    QTextDocument *text = documentFor(doc);
    return text ? text->toPlainText() : QString();
}
