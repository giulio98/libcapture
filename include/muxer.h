#pragma once

#include <iostream>
#include <string>

#include "ffmpeg_libs.h"

class Muxer {
    AVFormatContext *fmt_ctx_;
    std::string filename_;
    AVStream *video_stream_;
    AVStream *audio_stream_;
    bool file_opened_;
    bool file_closed_;
    bool time_base_valid_;

    /**
     * Write a packet to the output file
     * @param packet the packet to write. If nullptr, the output queue will be flushed
     * @param stream_index the index of the stream to which the packet belongs
     */
    void writePacket(std::shared_ptr<AVPacket> packet, int stream_index) const;

    void cleanup();

public:
    Muxer(const std::string &filename);

    ~Muxer();

    void addVideoStream(const AVCodecContext *codec_ctx);

    void addAudioStream(const AVCodecContext *codec_ctx);

    const AVCodecParameters *getVideoParams() const;

    const AVCodecParameters *getAudioParams() const;

    AVRational getVideoTimeBase() const;

    AVRational getAudioTimeBase() const;

    /**
     * Open the file and write the header
     */
    void openFile();

    /**
     * Write the trailer and close the file
     */
    void closeFile();

    /**
     * Write a video packet to the output file
     * @param packet the packet to write. If nullptr, the output queue will be flushed
     */
    void writeVideoPacket(std::shared_ptr<AVPacket> packet) const;

    /**
     * Write an audio packet to the output file
     * @param packet the packet to write. If nullptr, the output queue will be flushed
     */
    void writeAudioPacket(std::shared_ptr<AVPacket> packet) const;

    /**
     * Print informations
     */
    void dumpInfo() const;

    int getGlobalHeaderFlags() const;
};