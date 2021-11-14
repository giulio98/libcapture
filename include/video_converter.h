#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

class VideoConverter {
    int out_width_;
    int out_height_;
    AVPixelFormat out_pix_fmt_;
    SwsContext *ctx_;
    AVFrame *converted_frame_;

public:
    VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx);

    ~VideoConverter();

    const AVFrame *convertFrame(const AVFrame *in_frame, int frame_number) const;
};