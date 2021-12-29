#include "audio_converter.h"

#include <sstream>
#include <stdexcept>

AudioConverter::AudioConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx) {
    if (!dec_ctx) throwError("dec_ctx is NULL");
    if (!enc_ctx) throwError("enc_ctx is NULL");

    std::stringstream src_args_ss;
    src_args_ss << "time_base=" << enc_ctx->time_base.num << "/" << enc_ctx->time_base.den;
    src_args_ss << ":sample_rate=" << dec_ctx->sample_rate;
    src_args_ss << ":sample_fmt=" << av_get_sample_fmt_name(dec_ctx->sample_fmt);
    src_args_ss << ":channel_layout=" << dec_ctx->channel_layout;

    std::stringstream filter_spec_ss;
    /* number of samples for output frames */
    filter_spec_ss << "asetnsamples=n=" << enc_ctx->frame_size;
    /* format conversion */
    filter_spec_ss << ",aformat=sample_fmts=" << av_get_sample_fmt_name(enc_ctx->sample_fmt)
                   << ":sample_rates=" << enc_ctx->sample_rate
                   << ":channel_layouts=" << av_get_channel_name(enc_ctx->channel_layout);
    /* PTS */
    filter_spec_ss << ",asetpts=NB_CONSUMED_SAMPLES";

    init("abuffer", "abuffersink", src_args_ss.str(), filter_spec_ss.str());
}