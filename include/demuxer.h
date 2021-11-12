#pragma once

#include <string>

#include "decoder.h"
#include "ffmpeg_libs.h"

class Demuxer {
    AVFormatContext *fmt_ctx_;
    AVInputFormat *fmt_;
    std::string device_name_;
    AVDictionary *options_;
    AVStream *video_stream_;
    AVStream *audio_stream_;

public:
    Demuxer(const std::string &fmt_name, const std::string &device_name);

    ~Demuxer();

    void setOption(const std::string &key, const std::string &value);

    void open();

    AVStream *getVideoStream();

    AVStream *getAudioStream();

    void dumpInfo();
};