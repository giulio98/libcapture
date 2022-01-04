#pragma once

#include "common.h"
#include "encoder.h"

class AudioEncoder : public Encoder {
public:
    /**
     * Create a new audio encoder
     * @param codec_id              the ID of the codec to which encode the frames
     * @param options               the options to use for the encoder
     * @param dec_ctx               the context of the decoder (from which most of the params will be taken)
     * @param time_base             the time-base to use for the encoder (note that if this is different from the input
     * one, the timestamps of the packets/frames will have to be manually converted)
     * @param global_header_flags   the global header flags of the output format to which the encoded frames will be
     * sent
     */
    AudioEncoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, const AVCodecContext *dec_ctx,
                 int global_header_flags);
};