#include "video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options,
                           int global_header_flags, const AVCodecParameters *params, AVPixelFormat pix_fmt,
                           int framerate)
    : Encoder(codec_id) {
    codec_ctx_->width = params->width;
    codec_ctx_->height = params->height;
    codec_ctx_->pix_fmt = pix_fmt;
    codec_ctx_->gop_size = 2 * framerate;
    codec_ctx_->framerate.num = framerate;
    codec_ctx_->framerate.den = 1;
    codec_ctx_->time_base.num = 1;
    codec_ctx_->time_base.den = framerate;

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    open(options);
}