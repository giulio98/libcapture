#pragma once

#include <thread>

class ScopedThread {
    std::thread t_;

public:
    ScopedThread() noexcept = default;

    ScopedThread(std::thread &&t) noexcept : t_(std::move(t)) {}

    ScopedThread(const ScopedThread &) = delete;

    ScopedThread(ScopedThread &&other) noexcept : t_(std::move(other.t_)) {}

    ~ScopedThread() {
        if (t_.joinable()) t_.join();
    }

    ScopedThread &operator=(const ScopedThread &) = delete;

    ScopedThread &operator=(ScopedThread &&other) noexcept {
        t_ = std::move(other.t_);
        return *this;
    }
};