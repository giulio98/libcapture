#include "common/common.h"

class LogCallbackSetter {
public:
    LogCallbackSetter(void (*callback)(void *, int, const char *, va_list)) { av_log_set_callback(callback); }

    ~LogCallbackSetter() { av_log_set_callback(av_log_default_callback); }
};