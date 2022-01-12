#pragma once

#include <common/common.h>
#include <convert/converter.h>

class VideoConverter : public Converter {
public:
    /**
     * Create a new video converter converting the pixel-format and cropping the frame to the output size
     * WARNING: Even if the time-base of the encoder differs from the decoder's one, the timestamps of the frames
     * won't be converted (an eventual conversion will have to be performed separately)
     * @param dec_ctx       the decoder context containing the input params (time_base will be ignored)
     * @param enc_ctx       the encoder context containing the output params (time_base will be ignored)
     * @param in_time_base  the time-base of the frames sent to the converter
     * @param offset_x      the horizontal offset to use when performing the cropping of the frames
     * @param offset_y      the vertical offset to use when performing the cropping of the frames
     */
    VideoConverter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base,
                   int offset_x = 0, int offset_y = 0);
};