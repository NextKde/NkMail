#include "SingleInstance.h"
#include <QDBusConnection>
#include <QDBusMessage>

namespace nkmail {
namespace {
constexpr auto service = "org.NextKde.NkMail";
constexpr auto path = "/NkMail";
}
SingleInstance::~SingleInstance() {
    auto bus = QDBusConnection::sessionBus();
    if (m_primary) bus.unregisterService(service);
    if (m_registered) bus.unregisterObject(path);
}
SingleInstance::Result SingleInstance::acquire() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return Result::Error;
    m_registered = bus.registerObject(path, this, QDBusConnection::ExportAllSlots);
    if (!m_registered) return Result::Error;
    // The bus arbitrates ownership atomically, including concurrent launches.
    if (bus.registerService(service)) {
        m_primary = true;
        return Result::Primary;
    }
    auto request = QDBusMessage::createMethodCall(service, path, service, "Activate");
    return bus.call(request, QDBus::Block, 10000).type() == QDBusMessage::ReplyMessage
        ? Result::Activated : Result::Error;
}
void SingleInstance::Activate() { emit activationRequested(); }
}
