#pragma once

#include <memory>

#include "common.h"
#include "converter.h"

class VideoConverter : public Converter {
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
    VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx, int offset_x, int offset_y);

    /**
     * Send a frame to convert
     * @return always true, the return type is only for compatibility reasons
     */
    bool sendFrame(const AVFrame *frame) const;

    /**
     * Get a converted frame
     * @param frame_number the sequence number of the frame to use to compute the PTS
     * @return a new converted frame if it was possible to build it, nullptr otherwise
     */
    av::FrameUPtr getFrame(int64_t frame_number = 0) const;
};