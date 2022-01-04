#pragma once

#include <map>
#include <string>

#include "common.h"

class Encoder {
    AVCodec *codec_;
    av::CodecContextUPtr codec_ctx_;
    av::PacketUPtr packet_;

protected:
    explicit Encoder(AVCodecID codec_id);

    /**
     * Access the internal encoder
     * @return a pointer (to const) to the internal encoder
     */
    [[nodiscard]] const AVCodec *getCodec() const;

    /**
     * Access the internal codec context.
     * This function differs from getCodecContext() because the pointed context is modifiable (not const)
     * @return a pointer to access the codec context
     */
    [[nodiscard]] AVCodecContext *getCodecContextMod() const;

    /**
     * Initialize the internal codec context.
     * WARNING: This function must be called after setting the necessary context fields
     * @param options a map containing the options to use when initializing the context
     */
    void init(const std::map<std::string, std::string> &options);

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
    [[nodiscard]] av::PacketUPtr getPacket();

    /**
     * Access the internal codec context
     * @return an observer pointer to access the codec context
     */
    [[nodiscard]] const AVCodecContext *getCodecContext() const;

    /** Get the encoder name
     * @return the encoder name
     */
    [[nodiscard]] std::string getName() const;
};