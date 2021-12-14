#pragma once

#include <memory>

#include "common.h"

class VideoConverter {
    int in_width_;
    int in_height_;
    int out_width_;
    int out_height_;
    int offset_x_;
    int offset_y_;
    AVPixelFormat out_pix_fmt_;
    av::SwsContextUPtr sws_ctx_;
    av::FilterGraphUPtr filter_graph_;
    AVFilterContext *buffersrc_ctx_;
    AVFilterContext *buffersink_ctx_;

public:
    VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx, int offset_x,
                   int offset_y);

    // /**
    //  * Convert a frame
    //  * @param in_frame the frame to convert
    //  * @param frame_number the sequence number of the frame to use to compute the PTS
    //  * @return a new converted frame
    //  */
    // av::FrameUPtr convertFrame(const AVFrame *in_frame, int64_t frame_number = 0) const;

    void sendFrame(const AVFrame *frame) const;

    av::FrameUPtr getFrame(int64_t frame_number = 0) const;
};