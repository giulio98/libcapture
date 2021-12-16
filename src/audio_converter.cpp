#include "audio_converter.h"

#include <stdexcept>

static void throw_error(const std::string &msg) { throw std::runtime_error("Audio Converter: " + msg); }

AudioConverter::AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx) {
    if (!in_codec_ctx) throw_error("in_codec_ctx is NULL");
    if (!out_codec_ctx) throw_error("out_codec_ctx is NULL");

    out_channels_ = out_codec_ctx->channels;
    out_frame_size_ = out_codec_ctx->frame_size;
    out_sample_rate_ = out_codec_ctx->sample_rate;
    out_sample_fmt_ = out_codec_ctx->sample_fmt;

    resample_ctx_ =
        av::SwrContextUPtr(swr_alloc_set_opts(nullptr, av_get_default_channel_layout(out_channels_), out_sample_fmt_,
                                              out_sample_rate_, av_get_default_channel_layout(in_codec_ctx->channels),
                                              in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate, 0, nullptr));
    if (!resample_ctx_) throw_error("failed to allocate the resampling context");

    if (swr_init(resample_ctx_.get()) < 0) throw_error("failed to initialize the resampling context");

    int fifo_duration = 1;  // maximum duration of the FIFO buffer, in seconds
    fifo_buf_ =
        av::AudioFifoUPtr(av_audio_fifo_alloc(out_sample_fmt_, out_channels_, out_sample_rate_ * fifo_duration));
    if (!fifo_buf_) throw_error("failed to allocate FIFO buffer");
}

bool AudioConverter::sendFrame(const AVFrame *frame) const {
    if (!frame) throw_error("frame is not allocated");
    if (av_audio_fifo_space(fifo_buf_.get()) < frame->nb_samples) return false;

    uint8_t **buf = nullptr;

    try {
        if (av_samples_alloc_array_and_samples(&buf, nullptr, out_channels_, frame->nb_samples, out_sample_fmt_, 0) < 0)
            throw_error("failed to alloc samples by av_samples_alloc_array_and_samples.");

        if (swr_convert(resample_ctx_.get(), buf, frame->nb_samples, (const uint8_t **)frame->extended_data,
                        frame->nb_samples) < 0)
            throw_error("failed to convert samples");

        if (av_audio_fifo_write(fifo_buf_.get(), (void **)buf, frame->nb_samples) < 0)
            throw_error("failed to write to fifo");

        if (**buf) av_freep(&buf[0]);

    } catch (...) {
        if (**buf) av_freep(&buf[0]);
        throw;
    }

    return true;
}

av::FrameUPtr AudioConverter::getFrame(int64_t frame_number) const {
    /* not enough samples to build a frame */
    if (av_audio_fifo_size(fifo_buf_.get()) < out_frame_size_) return nullptr;

    av::FrameUPtr frame(av_frame_alloc());
    if (!frame) throw_error("failed to allocate internal frame");

    frame->nb_samples = out_frame_size_;
    frame->channels = out_channels_;
    frame->channel_layout = av_get_default_channel_layout(out_channels_);
    frame->format = out_sample_fmt_;
    frame->sample_rate = out_sample_rate_;
    if (av_frame_get_buffer(frame.get(), 0)) throw_error("failed to allocate frame data");

    if (av_audio_fifo_read(fifo_buf_.get(), (void **)frame->data, out_frame_size_) < 0)
        throw_error("failed to read data into frame");

    frame->pts = frame_number * out_frame_size_;

    return frame;
}