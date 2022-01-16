#pragma once

#include "common/common.h"

class Converter {
    av::FilterGraphUPtr filter_graph_;
    AVFilterContext *buffersrc_ctx_ = nullptr;
    AVFilterContext *buffersink_ctx_ = nullptr;
    av::FrameUPtr frame_;

    friend void swap(Converter &lhs, Converter &rhs);

public:
    /**
     * Create a new empty converter
     */
    Converter() = default;

    /**
     * Create a new audio/video converter.
     * For video, converting the pixel-format and cropping the frame to the output size.
     * For audio, converting the sample-format, sample-rate and channel layout.
     * WARNING: Even if the time-base of the encoder differs from the decoder's one, the timestamps of the frames
     * won't be converted (an eventual conversion will have to be performed separately)
     * @param dec_ctx       the decoder context containing the input params (time_base will be ignored)
     * @param enc_ctx       the encoder context containing the output params (time_base will be ignored)
     * @param in_time_base  the time-base of the frames sent to the converter
     * @param offset_x      (video-only) the horizontal offset to use when performing the cropping of the frames
     * @param offset_y      (video-only) the vertical offset to use when performing the cropping of the frames
     */
    Converter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base, int offset_x = 0,
              int offset_y = 0);

    Converter(const Converter &) = delete;

    Converter(Converter &&other) noexcept ;

    ~Converter() = default;

    Converter &operator=(Converter other);

    /**
     * Send a frame to convert
     */
    void sendFrame(av::FrameUPtr frame);

    /**
     * Get a converted frame
     * @return a new converted frame if it was possible to build it, nullptr otherwise
     */
    av::FrameUPtr getFrame();
};