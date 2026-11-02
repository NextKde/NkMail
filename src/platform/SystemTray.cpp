#include "TrayService.h"
#include <QAction>
#include <QCoreApplication>
#include <QEvent>
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
        auto *show = m_menu->addAction(QCoreApplication::translate("SystemTray", "Show NkMail"));
        show->setObjectName(QStringLiteral("showMailWindow"));
        m_menu->addSeparator();
        auto *quit = m_menu->addAction(QCoreApplication::translate("SystemTray", "Quit NkMail"));
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
        qApp->installEventFilter(this);
    }
    ~SystemTray() override { m_icon->hide(); m_icon->setContextMenu(nullptr); }
    bool available() const override { return QSystemTrayIcon::isSystemTrayAvailable(); }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == qApp && event->type() == QEvent::LanguageChange) {
            m_menu->actions().at(0)->setText(QCoreApplication::translate("SystemTray", "Show NkMail"));
            m_menu->actions().at(2)->setText(QCoreApplication::translate("SystemTray", "Quit NkMail"));
        }
        return TrayBackend::eventFilter(watched, event);
    }
private:
    std::unique_ptr<QMenu> m_menu;
    QSystemTrayIcon *m_icon;
};
}
std::unique_ptr<TrayBackend> createSystemTray() { return std::make_unique<SystemTray>(); }
}
