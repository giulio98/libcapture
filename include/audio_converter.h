#pragma once

#include "common.h"
#include "converter.h"

class AudioConverter : public Converter {
public:
    /**
     * Create a new audio converter converting the sample-format, sample-rate and channel layout.
     * WARNING: Even if the time-base of the encoder differs from the decoder's one, the timestamps of the frames
     * won't be converted (an eventual conversion will have to be performed separately)
     * @param dec_ctx       the decoder context containing the input params (time_base will be ignored)
     * @param enc_ctx       the encoder context containing the output params (time_base will be ignored)
     * @param in_time_base  the time-base of the frames sent to the converter
     */
    AudioConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base);
};