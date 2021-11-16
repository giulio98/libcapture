#pragma once

#include <iostream>

#include "deleter.h"
#include "ffmpeg_libs.h"

class AudioConverter {
    using unique_ptr_swr_ctx = std::unique_ptr<SwrContext, DeleterPP<swr_free>>;
    using unique_ptr_fifo = std::unique_ptr<AVAudioFifo, DeleterP<av_audio_fifo_free>>;

    int out_channels_;
    int out_frame_size_;
    int out_sample_rate_;
    AVSampleFormat out_sample_fmt_;
    unique_ptr_swr_ctx ctx_;
    unique_ptr_fifo fifo_buf_;
    int fifo_duration_;
    AVRational codec_ctx_time_base_;
    AVRational stream_time_base_;

public:
    AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx,
                   AVRational stream_time_base);

    ~AudioConverter();

    /**
     * Send a frame to convert
     * @return true if the conversion was successful, false, if the internal
     * buffer didn't have enough space to copy the input samples
     */
    bool sendFrame(std::shared_ptr<const AVFrame> frame) const;

    /**
     * Get a converted frame
     * @param frame_number the sequence number of the frame to use to compute the PTS
     * @return a new converted frame if it was possible to build it, nullptr if the internal buffer didn't have
     * enough samples to build a frame
     */
    std::shared_ptr<const AVFrame> getFrame(int64_t frame_number) const;
};