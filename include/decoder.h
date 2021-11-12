#pragma once

#include <iostream>

#include "exceptions.h"
#include "ffmpeg_libs.h"

class Decoder {
    AVCodec *codec_;
    AVCodecContext *codec_ctx_;

public:
    Decoder(const AVCodecParameters *params);

    ~Decoder();

    void sendPacket(AVPacket *packet);

    void fillFrame(AVFrame *frame);

    const AVCodecContext *getCodecContext();
};