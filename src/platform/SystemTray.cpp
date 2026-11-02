#include "TrayService.h"
#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace nkmail {
namespace {
class SystemTray final : public TrayBackend {
public:
    SystemTray() : m_menu(std::make_unique<QMenu>()), m_icon(new QSystemTrayIcon(this)) {
        m_icon->setIcon(QIcon(QStringLiteral(":/data/nkmail.svg")));
        m_icon->setToolTip(QStringLiteral("NkMail"));
        auto *show = m_menu->addAction(tr("显示 NkMail"));
        show->setObjectName(QStringLiteral("showMailWindow"));
        m_menu->addSeparator();
        auto *quit = m_menu->addAction(tr("退出 NkMail"));
        quit->setObjectName(QStringLiteral("quitMail"));
        connect(show, &QAction::triggered, this, &TrayBackend::showRequested);
        connect(quit, &QAction::triggered, this, &TrayBackend::quitRequested);
        connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                emit showRequested();
        });
        m_icon->setContextMenu(m_menu.get());
        // Qt registers automatically when a supported tray host becomes available.
        m_icon->show();
    }
    ~SystemTray() override { m_icon->hide(); m_icon->setContextMenu(nullptr); }
    bool available() const override { return QSystemTrayIcon::isSystemTrayAvailable(); }
private:
    std::unique_ptr<QMenu> m_menu;
    QSystemTrayIcon *m_icon;
};
}
std::unique_ptr<TrayBackend> createSystemTray() { return std::make_unique<SystemTray>(); }
}
