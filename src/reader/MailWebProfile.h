#pragma once
#include <QQuickWebEngineProfile>

class MailDocumentHandler;
void registerMailDocumentScheme();

// Mail pages never use the default browser profile or Chromium's network stack.
class MailWebProfile : public QQuickWebEngineProfile {
    Q_OBJECT
public:
    explicit MailWebProfile(QObject *parent = nullptr);
    Q_INVOKABLE QUrl setDocument(const QString &html);
private:
    MailDocumentHandler *m_documents;
};

// Compatibility adapter for WebEngine's retained scene-graph texture lifetime.
// Uses public QObject/Quick APIs; the delegate identity is private Qt knowledge.
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QSet>
#include <QHash>

class MailRenderLifecycle : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickItem *view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(bool attached READ attached NOTIFY attachedChanged)
public:
    explicit MailRenderLifecycle(QObject *parent = nullptr) : QObject(parent) {}
    QQuickItem *view() const { return m_view; }
    void setView(QQuickItem *view);
    bool attached() const { return m_attached; }
signals:
    void viewChanged();
    void attachedChanged();
private:
    void scheduleScan();
    void scan();
    QPointer<QQuickItem> m_view;
    QSet<QQuickItem *> m_watched;
    QHash<QQuickItem *, QPointer<QQuickWindow>> m_retained;
    bool m_scheduled = false;
    bool m_attached = false;
};
