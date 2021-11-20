#pragma once

#include <memory>

#include "common.h"

class AudioConverter {
    static const int64_t invalidNextPts = -1;
    int out_channels_;
    int out_frame_size_;
    int out_sample_rate_;
    AVSampleFormat out_sample_fmt_;
    av::SwrContextUPtr ctx_;
    av::AudioFifoUPtr fifo_buf_;
    AVRational in_time_base_;
    AVRational out_time_base_;
    int64_t next_pts_;
    int64_t fifo_duration_;

public:
    AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx, AVRational in_time_base);

    /**
     * Send a frame to convert
     * @return true if the conversion was successful, false if the internal
     * buffer didn't have enough space to copy the input samples
     */
    bool sendFrame(const AVFrame *frame, int64_t pts_offset = 0);

    /**
     * Get a converted frame
     * @param frame_number the sequence number of the frame to use to compute the PTS
     * @return a new converted frame if it was possible to build it, nullptr if the internal buffer didn't have
     * enough samples to build a frame
     */
    av::FrameUPtr getFrame();
};