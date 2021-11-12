#include "../include/video_encoder.h"

VideoEncoder::VideoEncoder(AVCodecID codec_id, std::map<std::string, std::string> options, int global_header_flags,
                           const AVCodecParameters *params, AVPixelFormat pix_fmt, int framerate)
    : Encoder(codec_id, options, global_header_flags) {
    codec_ctx_->width = params->width;
    codec_ctx_->height = params->height;
    codec_ctx_->pix_fmt = pix_fmt;
    codec_ctx_->framerate = (AVRational){framerate, 1};
    codec_ctx_->time_base = (AVRational){1, framerate};
}