#pragma once

#include "common.h"
#include "converter.h"

class VideoConverter : public Converter {
public:
    /**
     * Create a new video converter
     * @param dec_ctx       the codec context containing the input params (from the decoder)
     * @param enc_ctx       the codec context containing the output params (from the encoder)
     * @param in_time_base  the time-base of the input frames
     * @param offset_x      the horizontal offset to use when performing the cropping of the frames
     * @param offset_y      the vertical offset to use when performing the cropping of the frames
     */
    VideoConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base, int offset_x,
                   int offset_y);
};