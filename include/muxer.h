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

public:
    Muxer(const std::string &filename);

    ~Muxer();

    void addVideoStream(const AVCodecContext *codec_ctx);

    void addAudioStream(const AVCodecContext *codec_ctx);

    int getVideoStreamIdx();
    int getAudioStreamIdx();

    const AVCodecParameters *getVideoParams();
    const AVCodecParameters *getAudioParams();

    AVRational getVideoTimeBase();
    AVRational getAudioTimeBase();

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
     * The ownership of the packet is tranfered to the muxer
     */
    void writePacket(AVPacket *packet);

    /**
     * Print informations
     */
    void dumpInfo();

    int getGlobalHeaderFlags();
};