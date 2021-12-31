#include "audio_encoder.h"

AudioEncoder::AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options,
                           const AVCodecContext *dec_ctx, int global_header_flags)
    : Encoder(codec_id) {
    AVCodecContext *codec_ctx = getCodecContextMod();

    codec_ctx->sample_rate = dec_ctx->sample_rate;
    /* Take the channels from the decoder (NOT the channel layout, since it may not be set e.g. on Linux) */
    codec_ctx->channels = dec_ctx->channels;
    codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);
    codec_ctx->sample_fmt = getCodec()->sample_fmts[0];  // for aac there is AV_SAMPLE_FMT_FLTP = 8
    codec_ctx->time_base.num = 1;
    codec_ctx->time_base.den = codec_ctx->sample_rate;

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    init(options);
}