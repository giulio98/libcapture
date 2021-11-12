#pragma once

#include "ffmpeg_libs.h"

class Decoder {
    AVCodec *codec_;
    AVCodecContext *codec_ctx_;

public:
    Decoder(AVCodecID codec_id);

    ~Decoder();

    void setParamsFromStream(AVStream *stream);

    int sendPacket(AVPacket *packet);

    int fillFrame(AVFrame *frame);
};