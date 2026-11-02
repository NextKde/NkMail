#pragma once
#include <QThreadPool>
#include <QtConcurrent>
#include <functional>
namespace nkmail {
// One serial protocol queue per owner. Owners invalidate callbacks with QPointer.
class TaskRunner final {
public:
    TaskRunner() { m_pool.setMaxThreadCount(1); m_pool.setExpiryTimeout(-1); }
    ~TaskRunner() { finish(); }
    void run(std::function<void()> task) { (void)QtConcurrent::run(&m_pool, std::move(task)); }
    void finish() { m_pool.clear(); m_pool.waitForDone(); }
private:
    QThreadPool m_pool;
};
}
