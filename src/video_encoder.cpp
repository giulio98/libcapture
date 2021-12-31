#include "video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int width, int height,
                           AVPixelFormat pix_fmt, AVRational time_base, int global_header_flags)
    : Encoder(codec_id) {
    AVCodecContext *codec_ctx = getCodecContextMod();

    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->pix_fmt = pix_fmt;
    codec_ctx->time_base = time_base;

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    init(options);
}