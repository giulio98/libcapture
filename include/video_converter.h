#pragma once

#include <memory>

#include "common.h"

class VideoConverter {
    int out_width_;
    int out_height_;
    AVPixelFormat out_pix_fmt_;
    av::SwsContextUPtr ctx_;
    AVRational in_time_base_;
    AVRational out_time_base_;
    int64_t last_pts_;

public:
    VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx, AVRational in_time_base);

    /**
     * Convert a frame
     * @param in_frame the frame to convert
     * @param frame_number the sequence number of the frame to use to compute the PTS
     * @return a new converted frame
     */
    av::FrameUPtr convertFrame(const AVFrame *in_frame, int64_t pts_offset = 0) ;
};