#pragma once

#include "ffmpeg_libs.h"
#include "exceptions.h"

class Decoder {
    AVCodecContext *codec_ctx_;

public:
    Decoder(const AVCodecParameters *params);

    ~Decoder();

    void sendPacket(AVPacket *packet);

    void fillFrame(AVFrame *frame);

    int flush();
};