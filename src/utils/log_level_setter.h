#pragma once

#include "common/common.h"

class LogLevelSetter {
    const int prev_log_level_;

public:
    explicit LogLevelSetter(int log_level) : prev_log_level_(av_log_get_level()) { av_log_set_level(log_level); }

    ~LogLevelSetter() { av_log_set_level(prev_log_level_); }
};