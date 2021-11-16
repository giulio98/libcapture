#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

template <typename T>
class Deleter {
    void (*deleter_)(T **);

public:
    Deleter(void (*deleter)(T **)) : deleter_(deleter) {}
    void operator()(T *t) {
        // std::cout << "Deleter: " << t;
        deleter_(&t);
        // std::cout << " -> " << t << std::endl;
    }
};