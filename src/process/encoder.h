#pragma once

#include <map>
#include <string>

#include "common/common.h"

class Encoder {
    AVCodec *codec_{};
    av::CodecContextUPtr codec_ctx_;
    av::PacketUPtr packet_;

    friend void swap(Encoder &lhs, Encoder &rhs);

    /** Create a new encoder
     * @param codec_id the ID of the codec to which encode the frames
     */
    explicit Encoder(AVCodecID codec_id);

    /**
     * Initialize the internal codec context (note that a not-initialized encode won't be usable).
     * WARNING: This function must be called after setting the necessary context fields
     * @param options a map containing the options to use when initializing the context
     */
    void init(int global_header_flags, const std::map<std::string, std::string> &options);

public:
    /**
     * Create a new empty encoder
     */
    Encoder() = default;

    /**
     * Create a new AUDIO encoder
     * @param codec_id              the ID of the codec to which encode the frames
     * @param sample_rate           the sample rate of the audio to encode
     * @param channnel_layout       the channel layout of the frames to encode (if unknown, it can be obtained by
     * calling av_get_default_channel_layout(num_channels))
     * @param global_header_flags   the global header flags of the output format to which the encoded frames will be
     * sent
     * @param options               a map filled with the key-value options to use for the encoder
     */
    Encoder(AVCodecID codec_id, int sample_rate, uint64_t channel_layout, int global_header_flags,
            const std::map<std::string, std::string> &options);

    /**
     * Create a new VIDEO encoder
     * @param codec_id              the ID of the codec to which encode the frames
     * @param width                 the width of the video frames to encode
     * @param height                the height of the video frames to encode
     * @param pix_fmt               the pixel format of the video frames to encode
     * @param time_base             the time-base to use for the encoder (note that if this is different from the input
     * one, the timestamps of the packets/frames will have to be manually converted)
     * @param global_header_flags   the global header flags of the output format to which the encoded frames will be
     * sent
     * @param options               a map filled with the key-value options to use for the encoder
     */
    Encoder(AVCodecID codec_id, int width, int height, AVPixelFormat pix_fmt, AVRational time_base,
            int global_header_flags, const std::map<std::string, std::string> &options);

    Encoder(const Encoder &) = delete;

    Encoder(Encoder &&other) noexcept;

    ~Encoder() = default;

    Encoder &operator=(Encoder other);

    /**
     * Send a frame to the encoder
     * @param frame the frame to send to the encoder. It can be nullptr to flush the encoder
     * @return true if the frame has been correctly sent, false if the encoder could not receive it
     */
    bool sendFrame(const AVFrame *frame);

    /**
     * Get a converted packet from the encoder
     * @return a packet if it was possible to get it, nullptr if the encoder had nothing to write
     * because it is empty or flushed
     */
    av::PacketUPtr getPacket();

    /**
     * Access the internal codec context
     * @return an observer pointer to access the codec context
     */
    [[nodiscard]] const AVCodecContext *getContext() const;

    /** Get the encoder name
     * @return the encoder name
     */
    [[nodiscard]] std::string getName() const;
};