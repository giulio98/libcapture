#pragma once

#include <map>
#include <string>

#include "common.h"

class Encoder {
protected:
    AVCodec *codec_;
    av::CodecContextUPtr codec_ctx_;

    explicit Encoder(AVCodecID codec_id);

    void open(const std::map<std::string, std::string> &options);

public:
    /**
     * Send a frame to the encoder
     * @param frame the frame to send to the encoder. It can be nullptr to flush the encoder
     * @return true if the frame has been correctly sent, false if the encoder could not receive it
     */
    bool sendFrame(const AVFrame *frame) const;

    /**
     * Get a converted packet from the encoder
     * @return a packet if it was possible to get it, nullptr if the encoder had nothing to write
     * because it is empty or flushed
     */
    [[nodiscard]] av::PacketUPtr getPacket() const;

    [[nodiscard]] const AVCodecContext *getCodecContext() const;
};