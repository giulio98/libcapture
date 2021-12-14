#include "audio_encoder.h"

AudioEncoder::AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options,
                           int global_header_flags, int channels, int sample_rate)
    : Encoder(codec_id) {
    codec_ctx_->channels = channels;
    codec_ctx_->channel_layout = av_get_default_channel_layout(channels);
    codec_ctx_->sample_rate = sample_rate;
    codec_ctx_->sample_fmt = codec_->sample_fmts[0];  // for aac there is AV_SAMPLE_FMT_FLTP = 8
    // codec_ctx_->bit_rate = 96000;
    codec_ctx_->time_base.num = 1;
    codec_ctx_->time_base.den = sample_rate;

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    open(options);
}