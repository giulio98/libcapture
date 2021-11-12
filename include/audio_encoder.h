#pragma once

#include "encoder.h"
#include "ffmpeg_libs.h"

class AudioEncoder : public Encoder {
public:
    AudioEncoder(AVCodecID codec_id, std::map<std::string, std::string> options, int global_header_flags, int channels,
                 int sample_rate);
};