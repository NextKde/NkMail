#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>

class QQuickTextDocument;

class RichTextEditor : public QObject {
    Q_OBJECT
public:
    explicit RichTextEditor(QObject *parent = nullptr);

    Q_INVOKABLE void setSelection(int start, int end);
    Q_INVOKABLE void toggleBold(QQuickTextDocument *doc);
    Q_INVOKABLE void toggleItalic(QQuickTextDocument *doc);
    Q_INVOKABLE void toggleUnderline(QQuickTextDocument *doc);
    Q_INVOKABLE void toggleBulletList(QQuickTextDocument *doc);
    Q_INVOKABLE void toggleNumberedList(QQuickTextDocument *doc);
    Q_INVOKABLE void setAlignment(QQuickTextDocument *doc, int alignment);
    Q_INVOKABLE void changeIndent(QQuickTextDocument *doc, int delta);
    Q_INVOKABLE QVariantMap blockState(QQuickTextDocument *doc) const;
    Q_INVOKABLE int insertText(QQuickTextDocument *doc, const QString &value);
    Q_INVOKABLE void insertLink(QQuickTextDocument *doc, const QString &url);
    Q_INVOKABLE QString toHtml(QQuickTextDocument *doc) const;
    Q_INVOKABLE QString toPlainText(QQuickTextDocument *doc) const;
private:
    void toggleList(QQuickTextDocument *doc, bool numbered);
    int m_selectionStart = 0;
    int m_selectionEnd = 0;
};
