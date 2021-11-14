#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

class Decoder {
    AVCodec *codec_;
    AVCodecContext *codec_ctx_;

public:
    Decoder(const AVCodecParameters *params);

    ~Decoder();

    /**
     * Send an allocated packet to the decoder
     * The owneship of the packet remains to the caller
     * @return true if the packet has been correctly sent, false if the decoder could not receive it
     */
    bool sendPacket(const AVPacket *packet) const;

    /**
     * Fill an allocated frame obtained by decoding packets
     * The owneship of the frame remains to the caller
     * @return true if the frame has been correctly filled, false if the decoder had nothing to write
     */
    bool fillFrame(AVFrame *frame) const;

    const AVCodecContext *getCodecContext() const;
};