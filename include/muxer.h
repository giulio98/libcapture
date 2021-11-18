#pragma once

#include <memory>
#include <string>

#include "common.h"

class Muxer {
    av::FormatContextUPtr fmt_ctx_;
    std::string filename_;
    const AVStream *video_stream_;
    const AVStream *audio_stream_;
    AVRational video_codec_time_base_;
    AVRational audio_codec_time_base_;
    bool file_opened_;
    bool file_closed_;

public:
    Muxer(const std::string &filename);

    ~Muxer();

    void addVideoStream(const AVCodecContext *codec_ctx);

    void addAudioStream(const AVCodecContext *codec_ctx);

    /**
     * Open the file and write the header
     */
    void openFile();

    /**
     * Write the trailer and close the file
     */
    void closeFile();

    /**
     * Write a packet to the output file
     * @param packet the packet to write. If nullptr, the output queue will be flushed
     * @param packet_type the type of the packet (audio or video)
     */
    void writePacket(av::PacketUPtr packet, av::DataType packet_type) const;

    /**
     * Print informations
     */
    void dumpInfo() const;

    int getGlobalHeaderFlags() const;
};