#include <libavutil/time.h>

#include <iostream>
#include <string>

class DurationLogger {
    std::string msg_;
    int64_t start_time_;

public:
    DurationLogger(const std::string &msg) {
        msg_ = msg;
        start_time_ = av_gettime();
    }

    ~DurationLogger() {
        int64_t elapsed_time = (av_gettime() - start_time_) / 1000;
        std::cout << msg_ << elapsed_time << " ms";
    }
};