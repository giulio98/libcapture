#pragma once

#include <iostream>
#include <string>

#include "ffmpeg_libs.h"

class Muxer {
    AVFormatContext *fmt_ctx_;
    std::string filename_;
    AVStream *video_stream_;
    AVStream *audio_stream_;

public:
    Muxer(const std::string &filename);

    ~Muxer();

    void addVideoStream(const AVCodecContext *codec_ctx);

    void addAudioStream(const AVCodecContext *codec_ctx);

    const AVStream *getVideoStream();

    const AVStream *getAudioStream();

    /**
     * Open the file and write the header
     */
    void writeHeader();

    /**
     * Write a packet to the output file
     * The ownership of the packet is tranfered to the muxer
     */
    void writePacket(AVPacket *packet);

    /**
     * Write the trailer and close the file
     */
    void writeTrailer();

    /**
     * Print informations
     */
    void dumpInfo();

    int getGlobalHeaderFlags();
};