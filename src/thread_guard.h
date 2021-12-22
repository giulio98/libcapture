#include <thread>

class ThreadGuard {
    std::thread &t_;

public:
    ThreadGuard(std::thread &t) : t_(t) {}

    ~ThreadGuard() {
        if (t_.joinable()) t_.join();
    }

    ThreadGuard(ThreadGuard const &) = delete;

    ThreadGuard &operator=(ThreadGuard const &) = delete;
};