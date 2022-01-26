#pragma once

#include <array>
#include <mutex>
#include <string>

#include "common/common.h"

class Muxer {
    av::FormatContextUPtr fmt_ctx_;
    std::string filename_;
    std::array<const AVStream *, av::MediaType::NumTypes> streams_{};
    std::array<AVRational, av::MediaType::NumTypes> encoders_time_bases_{};
    bool file_inited_{};
    bool file_finalized_{};

public:
    /**
     * Create a new muxer
     * @param filename the name of the output file
     */
    explicit Muxer(std::string filename);

    Muxer(const Muxer &) = delete;

    ~Muxer();

    Muxer &operator=(const Muxer &) = delete;

    /**
     * Add a stream to the muxer.
     * WARNING: This function must be called before opening the file with init()
     * @param enc_ctx   the context of the encoder generating the packet stream
     */
    void addStream(const AVCodecContext *enc_ctx);

    /**
     * Open the output file and write the header.
     * WARNING: After calling this function, it won't be possible to add streams to the muxer
     */
    void initFile();

    /**
     * Write the trailer and close the file.
     * WARNING: After calling this function, it won't be possible to send other packets to the muxer
     */
    void finalizeFile();

    /**
     * Whether the file managed by the muxer has been initialized (and hence the muxer cannot be modifed anymore)
     * @return true if the file managed by the muxer has been initialized, false otherwise
     */
    [[nodiscard]] bool isInited() const;

    /**
     * Write a packet to the output file.
     * WARNING: the muxer must be initialized with init() in order to accept packets, otherwise
     * an exception will be thrown
     * @param packet        the packet to write. If nullptr, the output queue will be flushed
     * @param packet_type   the type of the packet (audio or video). If the packet is nullptr,
     * this parameter is irrelevant
     */
    void writePacket(av::PacketUPtr packet, av::MediaType packet_type);

    /**
     * Print informations about the streams
     */
    void printInfo() const;

    /**
     * Get the global header flags of the output format
     * @return the global header flags
     */
    [[nodiscard]] int getGlobalHeaderFlags() const;
};