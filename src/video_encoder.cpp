#include "video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int width, int height,
                           AVPixelFormat pix_fmt, int framerate, int global_header_flags)
    : Encoder(codec_id) {
    AVCodecContext *codec_ctx = getCodecContextMod();

    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->pix_fmt = pix_fmt;
    codec_ctx->gop_size = framerate;
    codec_ctx->framerate.num = framerate;
    codec_ctx->framerate.den = 1;
    codec_ctx->time_base = av_inv_q(codec_ctx->framerate);

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    init(options);
}