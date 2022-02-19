#pragma once

#include <thread>

class ThreadGuard {
    std::thread &t_;

public:
    ThreadGuard(std::thread &t) : t_(t) {}

    ~ThreadGuard() {
        if (t_.joinable()) t_.join();
    }

    ThreadGuard(const ThreadGuard &) = delete;

    ThreadGuard &operator=(const ThreadGuard &) = delete;
};