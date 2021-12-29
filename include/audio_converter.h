#pragma once

#include "common.h"
#include "converter.h"

class AudioConverter : public Converter {
public:
    /**
     * Create a new audio converter
     * @param dec_ctx  the codec context containing the input params (from the decoder)
     * @param enc_ctx the codec context containing the output params (from the encoder)
     */
    AudioConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx);
};