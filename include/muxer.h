#pragma once

#include <array>
#include <mutex>
#include <string>

#include "common.h"

class Muxer {
    av::FormatContextUPtr fmt_ctx_;
    std::string filename_;
    std::array<const AVStream *, av::DataType::NumDataTypes> streams_;
    std::array<AVRational, av::DataType::NumDataTypes> encoders_time_bases_;
    bool file_opened_;
    bool file_closed_;
    std::mutex m_;

public:
    /**
     * Create a new muxer
     * @param filename the name of the output file
     */
    explicit Muxer(std::string filename);

    ~Muxer();

    /**
     * Add a stream to the muxer.
     * WARNING: This function must be called before opening the file with openFile()
     * @param enc_ctx   the context of the encoder generating the packet stream
     * @param data_type the type of data of the stream
     */
    void addStream(const AVCodecContext *enc_ctx, av::DataType data_type);

    /**
     * Open the file and write the header.
     * WARNING: After calling this function, it won't be possible to add streams to the muxer
     */
    void openFile();

    /**
     * Write the trailer and close the file.
     * WARNING: After calling this function, it won't be possible to open the file again
     */
    void closeFile();

    /**
     * Write a packet to the output file. This function is thread-safe
     * @param packet        the packet to write. If nullptr, the output queue will be flushed
     * @param packet_type   the type of the packet (audio or video). If the packet is nullptr,
     * this parameter is irrelevant
     */
    void writePacket(av::PacketUPtr packet, av::DataType packet_type);

    /**
     * Print informations about the streams
     */
    void dumpInfo();

    /**
     * Get the global header flags of the output format
     * @return the global header flags
     */
    [[nodiscard]] int getGlobalHeaderFlags();
};