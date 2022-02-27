#pragma once

#include <string>

class VideoParameters {
    int width_ = 0;
    int height_ = 0;
    int offset_x_ = 0;
    int offset_y_ = 0;
    int framerate_ = 0;

    /**
     * Check if the value is greater or equal to the lower bound.
     * If this is not the case, throw an exception
     * @param name  the name of the attribute to check
     * @param val   the value of the attribute
     * @param bound the lower bound for the attribute
     */
    static void checkGE(const std::string &name, const int val, const int bound) {
        if (val < bound) throw std::invalid_argument(name + " must be >= " + std::to_string(bound));
    }

    /**
     * Check if the value is an even number.
     * If this is not the case, throw an exception
     * @param name  the name of the attribute to check
     * @param val   the value of the attribute
     */
    static void checkEven(const std::string &name, const int val) {
        if (val % 2) throw std::invalid_argument(name + " must be divisible by 2");
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
        checkGE("height", height, 0);
        checkEven("width", width);
        checkEven("height", height);
        width_ = width;
        height_ = height;
    }

    void setVideoOffset(int offset_x, int offset_y) {
        checkGE("horizontal offset", offset_x, 0);
        checkGE("vertical offset", offset_y, 0);
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