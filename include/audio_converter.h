#pragma once

#include <memory>

#include "common.h"

class AudioConverter {
    int out_channels_;
    int out_frame_size_;
    int out_sample_rate_;
    AVSampleFormat out_sample_fmt_;
    av::SwrContextUPtr ctx_;
    av::AudioFifoUPtr fifo_buf_;
    int fifo_duration_;

public:
    AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx);

    /**
     * Send a frame to convert
     * @return true if the conversion was successful, false if the internal
     * buffer didn't have enough space to copy the input samples
     */
    bool sendFrame(const AVFrame *frame) const;

    /**
     * Get a converted frame
     * @param frame_number the sequence number of the frame to use to compute the PTS
     * @return a new converted frame if it was possible to build it, nullptr if the internal buffer didn't have
     * enough samples to build a frame
     */
    av::FrameUPtr getFrame(int64_t frame_number) const;
};