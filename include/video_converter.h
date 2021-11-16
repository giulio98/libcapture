#pragma once

#include <iostream>

#include "deleter.h"
#include "ffmpeg_libs.h"

class VideoConverter {
    using unique_ptr_sws_ctx = std::unique_ptr<SwsContext, DeleterP<sws_freeContext>>;

    int out_width_;
    int out_height_;
    AVPixelFormat out_pix_fmt_;
    unique_ptr_sws_ctx ctx_;
    AVRational codec_ctx_time_base_;
    AVRational stream_time_base_;

public:
    VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx,
                   AVRational stream_time_base);

    ~VideoConverter();

    /**
     * Convert a frame
     * @param in_frame the frame to convert
     * @param frame_number the sequence number of the frame to use to compute the PTS
     * @return a new converted frame
     */
    std::shared_ptr<const AVFrame> convertFrame(std::shared_ptr<const AVFrame> in_frame, int64_t frame_number) const;
};