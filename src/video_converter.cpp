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

    ctx_ = sws_getContext(in_codec_ctx->width, in_codec_ctx->height, in_codec_ctx->pix_fmt, out_width_, out_height_,
                          out_pix_fmt_, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!ctx_) throw std::runtime_error("VideoConverter: failed to allocate context");
}

VideoConverter::~VideoConverter() {
    if (ctx_) sws_freeContext(ctx_);
}

AVFrame *VideoConverter::allocFrame() {
    AVFrame *frame = av_frame_alloc();
    if (!frame) throw std::runtime_error("VideoConverter: failed to allocate frame");

    if (av_image_alloc(frame->data, frame->linesize, out_width_, out_height_, out_pix_fmt_, 1) < 0)
        throw std::runtime_error("VideoConverter: failed to allocate img in converted frame");

    return frame;
}

void VideoConverter::freeFrame(AVFrame **frame_ptr) {
    if (!*frame_ptr) throw std::runtime_error("VideoConverter: frame is not allocated");
    av_freep(&(*frame_ptr)->data[0]);
    av_frame_free(frame_ptr);
}

void VideoConverter::convertFrame(const AVFrame *in_frame, AVFrame *out_frame, int frame_number) {
    if (!in_frame) throw std::runtime_error("VideoConverter: in_frame is not allocated");
    if (!out_frame) throw std::runtime_error("VideoConverter: out_frame is not allocated");
    if (sws_scale(ctx_, in_frame->data, in_frame->linesize, 0, out_height_, out_frame->data, out_frame->linesize) < 0)
        throw std::runtime_error("VideoEncoder: failed to convert frame");
    out_frame->format = out_pix_fmt_;
    out_frame->width = out_width_;
    out_frame->height = out_height_;
    out_frame->pts = av_rescale_q(frame_number, codec_ctx_time_base_, stream_time_base_);
}