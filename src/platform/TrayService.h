#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QWindow>
#include <functional>
#include <memory>

namespace nkmail {
// Desktop integration is separate from mailbox/sending state. The presentation
// layer supplies the exit guard; hiding never stops services or initiates a send.
class TrayBackend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool available() const = 0;
signals:
    void showRequested();
    void quitRequested();
};
std::unique_ptr<TrayBackend> createSystemTray();

class TrayService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
public:
    explicit TrayService(std::unique_ptr<TrayBackend> backend, QObject *parent = nullptr);
    ~TrayService() override;
    void attach(QWindow *window, std::function<bool()> prepareQuit);
    bool available() const { return m_available; }
    Q_INVOKABLE bool hideToTray();
    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void requestQuit();
    void refreshAvailability();
signals:
    void availableChanged();
    void exitRequested();
private:
    std::unique_ptr<TrayBackend> m_backend;
    QPointer<QWindow> m_window;
    std::function<bool()> m_prepareQuit;
    QTimer m_poll;
    bool m_available = false;
    bool m_maximized = false;
};
}
