#include <thread>

class ScopedThread {
    std::thread t_;

public:
    ScopedThread() = default;

    ScopedThread(std::thread &&t) : t_(std::move(t)) {}

    ScopedThread(const ScopedThread &) = delete;

    ScopedThread(ScopedThread &&src) : t_(std::move(src.t_)) {}

    ~ScopedThread() {
        if (t_.joinable()) t_.join();
    }

    ScopedThread &operator=(ScopedThread &&src) {
        if (t_.joinable()) throw std::logic_error("Active thread already present");
        t_ = std::move(src.t_);
        return *this;
    }

    ScopedThread &operator=(const ScopedThread &) = delete;
};