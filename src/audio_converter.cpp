#include "audio_converter.h"

#include <sstream>
#include <stdexcept>

AudioConverter::AudioConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base) {
    if (!dec_ctx) throwError("dec_ctx is NULL");
    if (!enc_ctx) throwError("enc_ctx is NULL");

    std::stringstream src_args_ss;
    src_args_ss << "time_base=" << in_time_base.num << "/" << in_time_base.den;
    src_args_ss << ":sample_rate=" << dec_ctx->sample_rate;
    src_args_ss << ":sample_fmt=" << av_get_sample_fmt_name(dec_ctx->sample_fmt);
    if (dec_ctx->channel_layout) { /* if a specific channel layout is known to the decoder, use it */
        src_args_ss << ":channel_layout=" << dec_ctx->channel_layout;
    } else { /* otherwise, just set the number of channels */
        src_args_ss << ":channels=" << dec_ctx->channels;
    }

    std::stringstream filter_spec_ss;
    /* set PTS */
    filter_spec_ss << "asetpts=PTS-STARTPTS";
    /*
     * convert sample_rate, sample_fmt and channel layout
     * the async=1 option tells the filter to add extra silence / stretch / squeeze the audio
     * to ensure that the timestamps are correct
     */
    filter_spec_ss << ",aresample=" << enc_ctx->sample_rate << ":async=1"
                   << ":out_sample_fmt=" << enc_ctx->sample_fmt << ":out_channel_layout=" << enc_ctx->channel_layout;
    /* ensure correct number of samples for output frames (even with injected silence) */
    filter_spec_ss << ",asetnsamples=n=" << enc_ctx->frame_size;

    /* format conversion (OLD, now use aresample instead) */
    // filter_spec_ss << ",aformat=sample_fmts=" << av_get_sample_fmt_name(enc_ctx->sample_fmt)
    //                << ":sample_rates=" << enc_ctx->sample_rate << ":channel_layouts=" << enc_ctx->channel_layout;

    init("abuffer", "abuffersink", src_args_ss.str(), filter_spec_ss.str());
}