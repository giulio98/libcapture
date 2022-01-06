#include "video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, int width, int height, AVPixelFormat pix_fmt, AVRational time_base,
                           int global_header_flags, const std::map<std::string, std::string> &options)
    : Encoder(codec_id) {
    AVCodecContext *enc_ctx = getContextMod();
    const AVCodec *codec = getCodec();

    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->pix_fmt = pix_fmt;
    enc_ctx->time_base = time_base;

    if (global_header_flags & AVFMT_GLOBALHEADER) enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    init(options);
}