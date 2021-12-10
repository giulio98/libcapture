#pragma once

#include "encoder.h"
#include "common.h"

class AudioEncoder : public Encoder {
public:
    AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int global_header_flags,
                 const AVCodecParameters *params);
};