#include "TrayService.h"
#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
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
    void setUnreadCount(int count) override {
        m_unread = qMax(0, count);
        QIcon icon(QStringLiteral(":/data/nkmail.svg"));
        if (m_unread > 0) {
            QPixmap image = icon.pixmap(64, 64);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            const QRectF badge(17, 32, 47, 31);
            painter.setPen(Qt::NoPen); painter.setBrush(QColor("#d93036"));
            painter.drawRoundedRect(badge, 15, 15);
            QFont font = painter.font(); font.setPixelSize(23); font.setBold(true); painter.setFont(font);
            painter.setPen(Qt::white);
            painter.drawText(badge, Qt::AlignCenter, m_unread > 99 ? QStringLiteral("99+") : QString::number(m_unread));
            painter.end(); icon = QIcon(image);
        }
        m_icon->setIcon(icon);
        m_icon->setToolTip(m_unread > 0
            ? QCoreApplication::translate("SystemTray", "NkMail · %1 unread in synced inboxes").arg(m_unread)
            : QCoreApplication::translate("SystemTray", "NkMail · No unread messages in synced inboxes"));
    }
    bool available() const override { return QSystemTrayIcon::isSystemTrayAvailable(); }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == qApp && event->type() == QEvent::LanguageChange) {
            setUnreadCount(m_unread);
            m_menu->actions().at(0)->setText(QCoreApplication::translate("SystemTray", "Show NkMail"));
            m_menu->actions().at(2)->setText(QCoreApplication::translate("SystemTray", "Quit NkMail"));
        }
        return TrayBackend::eventFilter(watched, event);
    }
private:
    int m_unread = 0;
    std::unique_ptr<QMenu> m_menu;
    QSystemTrayIcon *m_icon;
};
}
std::unique_ptr<TrayBackend> createSystemTray() { return std::make_unique<SystemTray>(); }
}
