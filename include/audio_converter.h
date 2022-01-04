#pragma once

#include "common.h"
#include "converter.h"

class AudioConverter : public Converter {
public:
    /**
     * Create a new audio converter
     * @param dec_ctx       the decoder context containing the input params
     * @param enc_ctx       the encoder context containing the output params
     * @param in_time_base  the time-base of the input frames (must be taken from the demuxer)
     */
    AudioConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base);
};