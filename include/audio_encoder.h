#pragma once

#include "common.h"
#include "encoder.h"

class AudioEncoder : public Encoder {
public:
    AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, const AVCodecContext *dec_ctx,
                 int global_header_flags);
};