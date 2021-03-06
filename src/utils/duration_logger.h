#pragma once

#include <iostream>
#include <string>
#include <utility>

#include "common/common.h"

class DurationLogger {
    std::string msg_;
    int64_t start_time_;
    bool new_line_;

public:
    explicit DurationLogger(std::string msg, bool new_line = true) : msg_(std::move(msg)), new_line_(new_line) {
        start_time_ = av_gettime();
    }

    ~DurationLogger() {
        int64_t elapsed_time = (av_gettime() - start_time_) / 1000;
        std::cout << msg_ << elapsed_time << " ms";
        if (new_line_) std::cout << std::endl;
    }
};