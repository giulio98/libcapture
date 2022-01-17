#pragma once

#include <string>

class VideoParameters {
    int width_ = 0;
    int height_ = 0;
    int offset_x_ = 0;
    int offset_y_ = 0;
    int framerate_ = 0;

    static void checkGE(const std::string &name, const int val, const int bound) {
        if (val < bound) throw std::runtime_error(name + " must be >= " + std::to_string(bound));
    }

    static void checkEven(const std::string &name, const int val) {
        if (val % 2) throw std::runtime_error(name + " must be divisible by 2");
    }

public:
    VideoParameters() = default;

    VideoParameters(int width, int height, int offset_x, int offset_y, int framerate) {
        setVideoSize(width, height);
        setVideoOffset(offset_x, offset_y);
        setFramerate(framerate);
    }

    void setVideoSize(int width, int height) {
        checkGE("width", width, 0);
        checkGE("width", height, 0);
        checkEven("width", width);
        checkEven("height", height);
        width_ = width;
        height_ = height;
    }

    void setVideoOffset(int offset_x, int offset_y) {
        checkGE("offset x", offset_x, 0);
        checkGE("offset y", offset_y, 0);
        offset_x_ = offset_x;
        offset_y_ = offset_y;
    }

    void setFramerate(int framerate) {
        checkGE("framerate", framerate, 1);
        framerate_ = framerate;
    }

    [[nodiscard]] std::pair<int, int> getVideoSize() const { return std::make_pair(width_, height_); }

    [[nodiscard]] std::pair<int, int> getVideoOffset() const { return std::make_pair(offset_x_, offset_y_); }

    [[nodiscard]] int getFramerate() const { return framerate_; }
};