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
     * The ownership of the packet is tranfered to the muxer
     */
    void writePacket(AVPacket *packet, int index) const;

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

    void writeVideoPacket(AVPacket *packet) const;

    void writeAudioPacket(AVPacket *packet) const;

    /**
     * Print informations
     */
    void dumpInfo() const;

    int getGlobalHeaderFlags() const;
};