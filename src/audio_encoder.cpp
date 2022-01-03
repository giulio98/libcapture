#include "audio_encoder.h"

AudioEncoder::AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options,
                           const AVCodecContext *dec_ctx, int global_header_flags)
    : Encoder(codec_id) {
    AVCodecContext *enc_ctx = getCodecContextMod();

    enc_ctx->sample_rate = dec_ctx->sample_rate;
    if (dec_ctx->channel_layout) { /* if a specific channel layout is known to the decoder, keep it */
        enc_ctx->channel_layout = dec_ctx->channel_layout;
        enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
    } else { /* otherwise, use the default channel layout */
        enc_ctx->channels = dec_ctx->channels;
        enc_ctx->channel_layout = av_get_default_channel_layout(enc_ctx->channels);
    }
    enc_ctx->sample_fmt = getCodec()->sample_fmts[0];
    /* for audio, the time base will be automatically set by init() */
    // enc_ctx->time_base.num = 1;
    // enc_ctx->time_base.den = enc_ctx->sample_rate;

    if (global_header_flags & AVFMT_GLOBALHEADER) enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    init(options);
}