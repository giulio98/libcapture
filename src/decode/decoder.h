#pragma once

#include <memory>

#include "common/common.h"

class Decoder {
    AVCodec *codec_ = nullptr;
    av::CodecContextUPtr codec_ctx_;
    av::FrameUPtr frame_;

public:
    Decoder() = default;

    /**
     * Create a new decoder
     * @param params the parameters of the stream to decode (likely taken from a demuxer)
     */
    explicit Decoder(const AVCodecParameters *params);

    Decoder(const Decoder &) = delete;

    Decoder(Decoder &&other);

    ~Decoder() = default;

    Decoder &operator=(const Decoder &) = delete;

    Decoder &operator=(Decoder &&other);

    /**
     * Send a packet to the decoder
     * @param packet the packet to send to the decoder. It can be nullptr to flush the decoder (WARNING:
     * sending more than one flush packet will throw an exception).
     * @return true if the packet has been correctly sent, false if the decoder could not receive it
     */
    bool sendPacket(const AVPacket *packet);

    /**
     * Get a converted Frame from the decoder
     * @return a frame if it was possible to get it, nullptr if the decoder had nothing to write
     * because it is empty or flushed
     */
    av::FrameUPtr getFrame();

    /**
     * Access the internal codec context
     * @return an observer pointer to access the codec context
     */
    [[nodiscard]] const AVCodecContext *getContext() const;

    /**
     * Get the name of the decoder
     * @return the name of the decoder
     */
    [[nodiscard]] std::string getName() const;
};