#pragma once

#include <memory>

#include "common.h"
#include "converter.h"

class AudioConverter : public Converter {
    av::FilterGraphUPtr filter_graph_;
    AVFilterContext *buffersrc_ctx_;
    AVFilterContext *buffersink_ctx_;

public:
    /**
     * Create a new audio converter
     * @param in_codec_ctx  the codec context containing the input params (from the decoder)
     * @param out_codec_ctx the codec context containing the output params (from the encoder)
     */
    AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx);

    /**
     * Send a frame to convert
     * @return true if the conversion was successful, false if the internal
     * buffer didn't have enough space to copy the input samples
     */
    void sendFrame(av::FrameUPtr frame) const override;

    /**
     * Get a converted frame
     * @param frame_number the frame's sequence number to use to compute the PTS
     * @return a new converted frame if it was possible to build it, nullptr if the internal buffer didn't have
     * enough samples to build a frame
     */
    [[nodiscard]] av::FrameUPtr getFrame() const override;
};