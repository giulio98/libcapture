#pragma once

#include "common/common.h"
#include "encoder.h"

class AudioEncoder : public Encoder {
public:
    AudioEncoder() = default;

    /**
     * Create a new audio encoder
     * @param codec_id              the ID of the codec to which encode the frames
     * @param sample_rate           the sample rate of the audio to encode
     * @param channnel_layout       the channel layout of the frames to encode (if unknown, it can be obtained by
     * calling av_get_default_channel_layout(num_channels))
     * @param global_header_flags   the global header flags of the output format to which the encoded frames will be
     * sent
     * @param options               a map filled with the key-value options to use for the encoder
     */
    AudioEncoder(AVCodecID codec_id, int sample_rate, uint64_t channel_layout, int global_header_flags,
                 const std::map<std::string, std::string> &options);

    AudioEncoder(const AudioEncoder &) = delete;

    AudioEncoder(AudioEncoder &&other);

    ~AudioEncoder() = default;

    AudioEncoder &operator=(const AudioEncoder &) = delete;

    AudioEncoder &operator=(AudioEncoder &&other);
};