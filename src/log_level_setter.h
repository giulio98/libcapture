#include "common.h"

class LogLevelSetter {
    int prev_log_level_;

public:
    LogLevelSetter(int log_level) {
        prev_log_level_ = av_log_get_level();
        av_log_set_level(log_level);
    }

    ~LogLevelSetter() { av_log_set_level(prev_log_level_); }
};