#include "video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int width, int height,
                           AVPixelFormat pix_fmt, int framerate, int global_header_flags)
    : Encoder(codec_id) {
    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->pix_fmt = pix_fmt;
    codec_ctx_->gop_size = framerate;
    codec_ctx_->framerate.num = framerate;
    codec_ctx_->framerate.den = 1;
    codec_ctx_->time_base = av_inv_q(codec_ctx_->framerate);

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    open(options);
}