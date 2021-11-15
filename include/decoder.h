#pragma once

#include <iostream>

#include "deleters.h"
#include "ffmpeg_libs.h"

class Decoder {
    AVCodec *codec_;
    AVCodecContext *codec_ctx_;

    void cleanup();

public:
    /**
     * Create a new decoder
     * @param params the parameters to use to initialize the decoder
     */
    Decoder(const AVCodecParameters *params);

    ~Decoder();

    /**
     * Send a packet to the decoder
     * @param packet the packet to send to the decoder. It can be nullptr to flush the decoder
     * @return true if the packet has been correctly sent, false if the decoder could not receive it
     */
    bool sendPacket(std::shared_ptr<const AVPacket> packet) const;

    /**
     * Get a converted Frame from the decoder
     * @return a frame if it was possible to get it, nullptr if the decoder had nothing to write
     * because it is empty or flushed
     */
    std::shared_ptr<const AVFrame> getFrame() const;

    const AVCodecContext *getCodecContext() const;
};