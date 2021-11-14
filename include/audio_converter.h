#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

class AudioConverter {
    int out_channels_;
    int out_frame_size_;
    int out_sample_rate_;
    AVSampleFormat out_sample_fmt_;
    SwrContext *ctx_;
    AVAudioFifo *fifo_buf_;
    int fifo_duration_;
    AVRational codec_ctx_time_base_;
    AVRational stream_time_base_;
    AVFrame *out_frame_;

    void cleanup();

public:
    AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx,
                   AVRational stream_time_base);

    ~AudioConverter();

    bool sendFrame(const AVFrame *frame);

    const AVFrame *getFrame(int frame_number);
};