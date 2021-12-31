#include "video_converter.h"

#include <sstream>
#include <stdexcept>

VideoConverter::VideoConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base,
                               int offset_x, int offset_y) {
    if (!dec_ctx) throwError("dec_ctx is NULL");
    if (!enc_ctx) throwError("enc_ctx is NULL");

    std::stringstream src_args_ss;
    src_args_ss << "video_size=" << dec_ctx->width << "x" << dec_ctx->height;
    src_args_ss << ":pix_fmt=" << dec_ctx->pix_fmt;
    src_args_ss << ":time_base=" << in_time_base.num << "/" << in_time_base.den;
    src_args_ss << ":pixel_aspect=" << dec_ctx->sample_aspect_ratio.num << "/" << dec_ctx->sample_aspect_ratio.den;

    std::stringstream filter_spec_ss;
    /* format conversion */
    filter_spec_ss << "format=" << enc_ctx->pix_fmt;
    /* cropping */
    filter_spec_ss << ",crop=" << enc_ctx->width << ":" << enc_ctx->height << ":" << offset_x << ":" << offset_y;
    /* PTS */
    filter_spec_ss << ",setpts=PTS-STARTPTS";

    init("buffer", "buffersink", src_args_ss.str(), filter_spec_ss.str());
}