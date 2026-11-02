#include "TrayService.h"
#include <QGuiApplication>

namespace nkmail {
TrayService::TrayService(std::unique_ptr<TrayBackend> backend, QObject *parent)
    : QObject(parent), m_backend(std::move(backend)) {
    connect(m_backend.get(), &TrayBackend::showRequested, this, &TrayService::showWindow);
    connect(m_backend.get(), &TrayBackend::quitRequested, this, &TrayService::requestQuit);
    connect(&m_poll, &QTimer::timeout, this, &TrayService::refreshAvailability);
    m_poll.start(2000);
    refreshAvailability();
}
TrayService::~TrayService() { QGuiApplication::setQuitOnLastWindowClosed(true); }
void TrayService::attach(QWindow *window, std::function<bool()> prepareQuit) {
    m_window = window;
    m_prepareQuit = std::move(prepareQuit);
}
void TrayService::refreshAvailability() {
    const bool available = m_backend->available();
    if (available == m_available) return;
    m_available = available;
    QGuiApplication::setQuitOnLastWindowClosed(!available);
    // If the panel/tray host disappears, never strand a hidden mail client.
    if (!available && m_window && !m_window->isVisible()) showWindow();
    emit availableChanged();
}
bool TrayService::hideToTray() {
    refreshAvailability();
    if (!m_available || !m_window) return false;
    m_maximized = m_window->visibility() == QWindow::Maximized;
    m_window->hide();
    return true;
}
void TrayService::showWindow() {
    if (!m_window) return;
    if (!m_window->isVisible() || m_window->visibility() == QWindow::Minimized) {
        if (m_maximized) m_window->showMaximized();
        else m_window->showNormal();
    }
    m_window->raise();
    m_window->requestActivate();
}
void TrayService::requestQuit() {
    if (!m_window || !m_prepareQuit) return;
    if (!m_prepareQuit()) { showWindow(); return; }
    emit exitRequested();
}
}
