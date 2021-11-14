#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

class VideoConverter {
    int out_width_;
    int out_height_;
    AVPixelFormat out_pix_fmt_;
    SwsContext *ctx_;

public:
    VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx);

    ~VideoConverter();

    AVFrame *allocFrame();

    void freeFrame(AVFrame **frame_ptr);

    void convertFrame(const AVFrame *in_frame, AVFrame *out_frame);
};