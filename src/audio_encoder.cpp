#include "audio_encoder.h"

AudioEncoder::AudioEncoder(AVCodecID codec_id, int sample_rate, uint64_t channel_layout, int global_header_flags,
                 const std::map<std::string, std::string> &options)
    : Encoder(codec_id) {
    AVCodecContext *enc_ctx = getContextMod();

    enc_ctx->sample_rate = sample_rate;
    enc_ctx->channel_layout = channel_layout;
    enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
    enc_ctx->sample_fmt = getCodec()->sample_fmts[0];
    /* for audio, the time base will be automatically set by init() */
    // enc_ctx->time_base.num = 1;
    // enc_ctx->time_base.den = enc_ctx->sample_rate;

    if (global_header_flags & AVFMT_GLOBALHEADER) enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    init(options);
}