#include "../include/video_converter.h"

VideoConverter::VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx,
                               AVRational stream_time_base)
    : ctx_(nullptr), stream_time_base_(stream_time_base) {
    if (!in_codec_ctx) throw std::runtime_error("VideoConverter: in_codec_ctx is NULL");
    if (!out_codec_ctx) throw std::runtime_error("VideoConverter: out_codec_ctx is NULL");

    out_width_ = out_codec_ctx->width;
    out_height_ = out_codec_ctx->height;
    out_pix_fmt_ = out_codec_ctx->pix_fmt;
    codec_ctx_time_base_ = out_codec_ctx->time_base;

    ctx_ =
        av::SwsContextPtr(sws_getContext(in_codec_ctx->width, in_codec_ctx->height, in_codec_ctx->pix_fmt, out_width_,
                                         out_height_, out_pix_fmt_, SWS_BICUBIC, nullptr, nullptr, nullptr));
    if (!ctx_) throw std::runtime_error("VideoConverter: failed to allocate context");
}

std::shared_ptr<const AVFrame> VideoConverter::convertFrame(std::shared_ptr<const AVFrame> in_frame,
                                                            int64_t frame_number) const {
    if (!in_frame) throw std::runtime_error("VideoConverter: in_frame is not allocated");

    std::shared_ptr<AVFrame> out_frame(av_frame_alloc(), DeleterPP<av_frame_free>());
    if (!out_frame) throw std::runtime_error("VideoConverter: failed to allocate frame");

    out_frame->width = out_width_;
    out_frame->height = out_height_;
    out_frame->format = out_pix_fmt_;
    if (av_frame_get_buffer(out_frame.get(), 0))
        throw std::runtime_error("VideoConverter: failed to allocate frame data");

    if (sws_scale(ctx_.get(), in_frame->data, in_frame->linesize, 0, out_height_, out_frame->data,
                  out_frame->linesize) < 0)
        throw std::runtime_error("VideoEncoder: failed to convert frame");

    out_frame->pts = av_rescale_q(frame_number, codec_ctx_time_base_, stream_time_base_);

    return out_frame;
}