#pragma once

#include <mutex>
#include <string>

#include "common.h"

class Muxer {
    av::FormatContextUPtr fmt_ctx_;
    std::string filename_;
    const AVStream *video_stream_;
    const AVStream *audio_stream_;
    AVRational video_codec_time_base_{};
    AVRational audio_codec_time_base_{};
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
     * Add a video stream to the muxer.
     * WARNING: This function must be called before opening the file with openFile()
     * @param codec_ctx a codec context containing the video parameters
     */
    void addVideoStream(const AVCodecContext *codec_ctx);

    /**
     * Add an audio stream to the muxer.
     * WARNING: This function must be called before opening the file with openFile()
     * @param codec_ctx a codec context containing the audio parameters
     */
    void addAudioStream(const AVCodecContext *codec_ctx);

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
    void dumpInfo() const;

    /**
     * Get the global header flags of the output format
     * @return the global header flags
     */
    [[nodiscard]] int getGlobalHeaderFlags() const;
};