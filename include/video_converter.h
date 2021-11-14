#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

class VideoConverter {
    int out_width_;
    int out_height_;
    AVPixelFormat out_pix_fmt_;
    SwsContext *ctx_;
    AVRational codec_ctx_time_base_;
    AVRational stream_time_base_;

public:
    VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx,
                   AVRational stream_time_base);

    ~VideoConverter();

    AVFrame *allocFrame() const;

    void freeFrame(AVFrame **frame_ptr) const;

    void convertFrame(const AVFrame *in_frame, AVFrame *out_frame, int frame_number) const;
};