#pragma once

#include "common.h"
#include "encoder.h"

class AudioEncoder : public Encoder {
public:
    AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int global_header_flags,
                 int channels, int sample_rate);
};