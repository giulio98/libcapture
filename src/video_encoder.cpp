#include "video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int width, int height,
                           AVPixelFormat pix_fmt, AVRational time_base, int global_header_flags)
    : Encoder(codec_id) {
    AVCodecContext *enc_ctx = getCodecContextMod();

    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->pix_fmt = pix_fmt;
    enc_ctx->time_base = time_base;

    if (global_header_flags & AVFMT_GLOBALHEADER) enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    init(options);
}