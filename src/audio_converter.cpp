#include "include/audio_converter.h"
#include <stdexcept>

AudioConverter::AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx)
    : ctx_(nullptr), fifo_buf_(nullptr), fifo_duration_(1) {
    if (!in_codec_ctx) throw std::runtime_error("AudioConverter: in_codec_ctx is NULL");
    if (!out_codec_ctx) throw std::runtime_error("AudioConverter: out_codec_ctx is NULL");

    out_channels_ = out_codec_ctx->channels;
    out_frame_size_ = out_codec_ctx->frame_size;
    out_sample_rate_ = out_codec_ctx->sample_rate;
    out_sample_fmt_ = out_codec_ctx->sample_fmt;

    ctx_ =
        av::SwrContextUPtr(swr_alloc_set_opts(nullptr, av_get_default_channel_layout(out_channels_), out_sample_fmt_,
                                              out_sample_rate_, av_get_default_channel_layout(in_codec_ctx->channels),
                                              in_codec_ctx->sample_fmt, in_codec_ctx->sample_rate, 0, nullptr));
    if (!ctx_) throw std::runtime_error("AudioConverter: failed to allocate context");

    if (swr_init(ctx_.get()) < 0) throw std::runtime_error("AudioConverter: failed to initialize context");

    fifo_buf_ =
        av::AudioFifoUPtr(av_audio_fifo_alloc(out_sample_fmt_, out_channels_, out_sample_rate_ * fifo_duration_));
    if (!fifo_buf_) throw std::runtime_error("AudioConverter: failed to allocate FIFO buffer");
}

bool AudioConverter::sendFrame(const AVFrame *frame) const {
    if (!frame) throw std::runtime_error("AudioConverter: frame is not allocated");
    if (av_audio_fifo_space(fifo_buf_.get()) < frame->nb_samples) return false;

    uint8_t **buf = nullptr;

    try {
        if (av_samples_alloc_array_and_samples(&buf, nullptr, out_channels_, frame->nb_samples, out_sample_fmt_, 0) < 0)
            throw std::runtime_error("AudioConverter: failed to alloc samples by av_samples_alloc_array_and_samples.");

        if (swr_convert(ctx_.get(), buf, frame->nb_samples, (const uint8_t **)frame->extended_data, frame->nb_samples) <
            0)
            throw std::runtime_error("AudioConverter: failed to convert samples");

        if (av_audio_fifo_write(fifo_buf_.get(), (void **)buf, frame->nb_samples) < 0)
            throw std::runtime_error("AudioConverter: failed to write to fifo");

        if (**buf) av_freep(&buf[0]);

    } catch (const std::exception &e) {
        if (**buf) av_freep(&buf[0]);
        throw;
    }

    return true;
}

av::FrameUPtr AudioConverter::getFrame(int64_t frame_number) const {
    /* not enough samples to build a frame */
    if (av_audio_fifo_size(fifo_buf_.get()) < out_frame_size_) return nullptr;

    av::FrameUPtr out_frame(av_frame_alloc());
    if (!out_frame) throw std::runtime_error("AudioConverter: failed to allocate internal frame");

    out_frame->nb_samples = out_frame_size_;
    out_frame->channels = out_channels_;
    out_frame->channel_layout = av_get_default_channel_layout(out_channels_);
    out_frame->format = out_sample_fmt_;
    out_frame->sample_rate = out_sample_rate_;
    if (av_frame_get_buffer(out_frame.get(), 0))
        throw std::runtime_error("AudioConverter: failed to allocate frame data");

    if (av_audio_fifo_read(fifo_buf_.get(), (void **)out_frame->data, out_frame_size_) < 0)
        throw std::runtime_error("AudioConverter: failed to read data into frame");

    out_frame->pts = frame_number * out_frame_size_;

    return out_frame;
}