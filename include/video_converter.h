#pragma once

#include "common.h"
#include "converter.h"

class VideoConverter : public Converter {
public:
    /**
     * Create a new video converter
     * @param dec_ctx       the decoder context containing the input params
     * @param enc_ctx       the encoder context containing the output params
     * @param in_time_base  the time-base of the input frames (must be taken from the demuxer)
     * @param offset_x      the horizontal offset to use when performing the cropping of the frames
     * @param offset_y      the vertical offset to use when performing the cropping of the frames
     */
    VideoConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base, int offset_x,
                   int offset_y);
};