#include "audio_encoder.h"

AudioEncoder::AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options,
                           const AVCodecContext *dec_ctx, int global_header_flags)
    : Encoder(codec_id) {
    codec_ctx_->sample_rate = dec_ctx->sample_rate;
    codec_ctx_->channel_layout = dec_ctx->channel_layout;
    codec_ctx_->channels = av_get_channel_layout_nb_channels(codec_ctx_->channel_layout);
    codec_ctx_->sample_fmt = codec_->sample_fmts[0];  // for aac there is AV_SAMPLE_FMT_FLTP = 8
    codec_ctx_->time_base.num = 1;
    codec_ctx_->time_base.den = codec_ctx_->sample_rate;

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    open(options);
}