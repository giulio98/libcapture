#pragma once

#include <map>
#include <string>

#include "exceptions.h"
#include "ffmpeg_libs.h"

class Encoder {
protected:
    AVCodec *codec_;
    AVCodecContext *codec_ctx_;

    Encoder(AVCodecID codec_id, std::map<std::string, std::string> options, int global_header_flags);

public:
    ~Encoder();

    void sendFrame(AVFrame *frame);

    void fillPacket(AVPacket *packet);

    const AVCodecContext *getCodecContext();
};