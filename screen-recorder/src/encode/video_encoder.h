#pragma once

#include <common/common.h>

#include "encoder.h"

class VideoEncoder : public Encoder {
public:
    /**
     * Create a new video encoder
     * @param codec_id              the ID of the codec to which encode the frames
     * @param width                 the width of the video frames to encode
     * @param height                the height of the video frames to encode
     * @param pix_fmt               the pixel format of the video frames to encode
     * @param time_base             the time-base to use for the encoder (note that if this is different from the input
     * one, the timestamps of the packets/frames will have to be manually converted)
     * @param global_header_flags   the global header flags of the output format to which the encoded frames will be
     * sent
     * @param options               a map filled with the key-value options to use for the encoder
     */
    VideoEncoder(AVCodecID codec_id, int width, int height, AVPixelFormat pix_fmt, AVRational time_base,
                 int global_header_flags, const std::map<std::string, std::string> &options);
};