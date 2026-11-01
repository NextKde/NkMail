#pragma once
#include <QObject>
#include <QString>

class QQuickTextDocument;

class RichTextEditor : public QObject {
    Q_OBJECT
public:
    explicit RichTextEditor(QObject *parent = nullptr);

    Q_INVOKABLE void toggleBold(QQuickTextDocument *doc);
    Q_INVOKABLE void toggleItalic(QQuickTextDocument *doc);
    Q_INVOKABLE void toggleUnderline(QQuickTextDocument *doc);
    Q_INVOKABLE void toggleBulletList(QQuickTextDocument *doc);
    Q_INVOKABLE void insertLink(QQuickTextDocument *doc, const QString &url);
    Q_INVOKABLE QString toHtml(QQuickTextDocument *doc) const;
    Q_INVOKABLE QString toPlainText(QQuickTextDocument *doc) const;
};
