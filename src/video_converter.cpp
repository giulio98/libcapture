#include "../include/video_converter.h"

VideoConverter::VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx,
                               AVRational stream_time_base)
    : ctx_(nullptr), converted_frame_(nullptr), stream_time_base_(stream_time_base) {
    if (!in_codec_ctx) throw std::runtime_error("VideoConverter: in_codec_ctx is NULL");
    if (!out_codec_ctx) throw std::runtime_error("VideoConverter: out_codec_ctx is NULL");

    out_width_ = out_codec_ctx->width;
    out_height_ = out_codec_ctx->height;
    out_pix_fmt_ = out_codec_ctx->pix_fmt;
    codec_ctx_time_base_ = out_codec_ctx->time_base;

    ctx_ = sws_getContext(in_codec_ctx->width, in_codec_ctx->height, in_codec_ctx->pix_fmt, out_width_, out_height_,
                          out_pix_fmt_, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!ctx_) throw std::runtime_error("VideoConverter: failed to allocate context");

    converted_frame_ = av_frame_alloc();
    if (!converted_frame_) throw std::runtime_error("VideoConverter: failed to allocate frame");
    converted_frame_->format = out_pix_fmt_;
    converted_frame_->width = out_width_;
    converted_frame_->height = out_height_;
    if (av_frame_get_buffer(converted_frame_, 0))
        throw std::runtime_error("VideoConverter: failed to allocate frame data");
}

VideoConverter::~VideoConverter() {
    if (ctx_) sws_freeContext(ctx_);
    if (converted_frame_) av_frame_free(&converted_frame_);
}

const AVFrame *VideoConverter::convertFrame(const AVFrame *in_frame, int frame_number) const {
    if (!in_frame) throw std::runtime_error("VideoConverter: in_frame is not allocated");
    if (sws_scale(ctx_, in_frame->data, in_frame->linesize, 0, out_height_, converted_frame_->data,
                  converted_frame_->linesize) < 0)
        throw std::runtime_error("VideoEncoder: failed to convert frame");
    converted_frame_->pts = av_rescale_q(frame_number, codec_ctx_time_base_, stream_time_base_);
    return converted_frame_;
}