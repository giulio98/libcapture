#include "../include/video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, std::map<std::string, std::string> options, int global_header_flags,
                           const AVCodecParameters *params, AVPixelFormat pix_fmt, int framerate)
    : Encoder(codec_id, options, global_header_flags) {
    codec_ctx_->width = params->width;
    codec_ctx_->height = params->height;
    codec_ctx_->pix_fmt = pix_fmt;
    codec_ctx_->framerate = (AVRational){framerate, 1};
    codec_ctx_->time_base = (AVRational){1, framerate};

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(codec_ctx_, codec_, options_ ? &options_ : nullptr);
    if (ret) throw std::runtime_error("Failed to initialize Codec Context");
}