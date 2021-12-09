#pragma once

#include "common.h"
#include "encoder.h"

class VideoEncoder : public Encoder {
public:
    VideoEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int global_header_flags,
                 const AVCodecParameters *params, AVPixelFormat pix_fmt, int framerate);
};