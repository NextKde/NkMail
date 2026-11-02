#pragma once
#include <QObject>

namespace nkmail {
// Desktop ownership is independent of mailbox storage and synthetic data paths.
class SingleInstance final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.NextKde.NkMail")
public:
    enum class Result { Primary, Activated, Error };
    explicit SingleInstance(QObject *parent = nullptr) : QObject(parent) {}
    ~SingleInstance() override;
    Result acquire();
public slots:
    void Activate();
signals:
    void activationRequested();
private:
    bool m_primary = false;
    bool m_registered = false;
};
}
