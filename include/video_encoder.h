#pragma once

#include "encoder.h"
#include "ffmpeg_libs.h"

class VideoEncoder : public Encoder {
public:
    VideoEncoder(AVCodecID codec_id, std::map<std::string, std::string> options, int global_header_flags,
                 const AVCodecParameters *params, AVPixelFormat pix_fmt, int framerate);
};