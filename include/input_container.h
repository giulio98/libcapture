#pragma once

#include <string>

#include "ffmpeg_libs.h"

class InputContainer {
    AVFormatContext *fmt_ctx_;
    AVInputFormat *fmt_;
    std::string device_name_;
    AVDictionary *options_;
    AVStream *video_stream_;
    AVStream *audio_stream_;

public:
    InputContainer(const std::string &fmt_name, const std::string &device_name);
    ~InputContainer();
    void setOption(const std::string &key, const std::string &value);
    void open();
    void dumpInfo();
};