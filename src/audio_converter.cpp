#include "../include/audio_converter.h"

AudioConverter::AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx,
                               AVRational stream_time_base)
    : ctx_(nullptr), fifo_buf_(nullptr), out_frame_(nullptr), stream_time_base_(stream_time_base), fifo_duration_(1) {
    if (!in_codec_ctx) throw std::runtime_error("AudioConverter: in_codec_ctx is NULL");
    if (!out_codec_ctx) throw std::runtime_error("AudioConverter: out_codec_ctx is NULL");

    out_channels_ = out_codec_ctx->channels;
    out_frame_size_ = out_codec_ctx->frame_size;
    out_sample_rate_ = out_codec_ctx->sample_rate;
    out_sample_fmt_ = out_codec_ctx->sample_fmt;
    codec_ctx_time_base_ = out_codec_ctx->time_base;

    ctx_ = swr_alloc_set_opts(nullptr, av_get_default_channel_layout(out_channels_), out_sample_fmt_, out_sample_rate_,
                              av_get_default_channel_layout(in_codec_ctx->channels), in_codec_ctx->sample_fmt,
                              in_codec_ctx->sample_rate, 0, nullptr);
    if (!ctx_) throw std::runtime_error("AudioConverter: failed to allocate context");

    if (swr_init(ctx_) < 0) throw std::runtime_error("AudioConverter: failed to initialize context");

    fifo_buf_ = av_audio_fifo_alloc(out_sample_fmt_, out_channels_, out_sample_rate_ * fifo_duration_);
    if (!fifo_buf_) throw std::runtime_error("AudioConverter: failed to allocate FIFO buffer");

    out_frame_ = av_frame_alloc();
    if (!out_frame_) throw std::runtime_error("AudioConverter: failed to allocate internal frame");
    out_frame_->nb_samples = out_frame_size_;
    out_frame_->channels = out_channels_;
    out_frame_->channel_layout = av_get_default_channel_layout(out_channels_);
    out_frame_->format = out_sample_fmt_;
    out_frame_->sample_rate = out_sample_rate_;
    if (av_frame_get_buffer(out_frame_, 0)) throw std::runtime_error("AudioConverter: failed to allocate frame data");
}

AudioConverter::~AudioConverter() {
    if (ctx_) swr_free(&ctx_);
    if (fifo_buf_) av_audio_fifo_free(fifo_buf_);
    if (out_frame_) av_frame_free(&out_frame_);
}

bool AudioConverter::sendFrame(const AVFrame *frame) {
    uint8_t **buf = nullptr;

    try {
        bool ret;

        if (!frame) throw std::runtime_error("AudioConverter: frame is not allocated");

        if (av_samples_alloc_array_and_samples(&buf, nullptr, out_channels_, out_frame_size_, out_sample_fmt_, 0) < 0)
            throw std::runtime_error("AudioConverter: failed to alloc samples by av_samples_alloc_array_and_samples.");

        if (swr_convert(ctx_, buf, frame->nb_samples, (const uint8_t **)frame->extended_data, frame->nb_samples) < 0)
            throw std::runtime_error("AudioConverter: failed to convert samples");

        if (av_audio_fifo_space(fifo_buf_) >= frame->nb_samples) {
            if (av_audio_fifo_write(fifo_buf_, (void **)buf, frame->nb_samples) < 0)
                throw std::runtime_error("AudioConverter: failed to write to fifo");
            ret = true;
        } else {
            ret = false;
        }

        if (**buf) av_freep(&buf[0]);

        return ret;

    } catch (const std::exception &e) {
        if (**buf) av_freep(&buf[0]);
        throw;
    }
}

const AVFrame *AudioConverter::getFrame(int frame_number) {
    if (av_audio_fifo_size(fifo_buf_) < out_frame_size_) return nullptr;
    if (av_audio_fifo_read(fifo_buf_, (void **)out_frame_->data, out_frame_size_) < 0)
        throw std::runtime_error("AudioConverter: failed to read data into frame");
    out_frame_->pts = av_rescale_q(out_frame_size_ * frame_number, codec_ctx_time_base_, stream_time_base_);
    return out_frame_;
}