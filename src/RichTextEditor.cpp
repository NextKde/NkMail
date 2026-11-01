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
}

RichTextEditor::RichTextEditor(QObject *parent) : QObject(parent) {}

void RichTextEditor::toggleBold(QQuickTextDocument *doc) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor(text);
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
    QTextCursor cursor(text);
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
    QTextCursor cursor(text);
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

void RichTextEditor::toggleBulletList(QQuickTextDocument *doc) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor(text);

    if (cursor.currentList()) {
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
    listFormat.setStyle(QTextListFormat::ListDisc);
    listFormat.setIndent(1);
    cursor.createList(listFormat);
    cursor.clearSelection();
}

void RichTextEditor::insertLink(QQuickTextDocument *doc, const QString &url) {
    QTextDocument *text = documentFor(doc);
    if (!text) return;
    QTextCursor cursor(text);
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
