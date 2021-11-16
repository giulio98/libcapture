#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

/**
 * auto will accept any signature for the "deleter" function (e.g. void (*deleter)(AVFrame **)),
 * creating a different struct for each one of them at compile-time
 */
template <auto deleter>
struct Deleter {
    template <typename T>
    void operator()(T* t) const {
        deleter(&t);
    }
};

// template <typename T, auto deleter>
// using unique_ptr_deleter = std::unique_ptr<T, Deleter<deleter>>;