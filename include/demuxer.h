#pragma once

#include <map>
#include <string>

#include "decoder.h"
#include "ffmpeg_libs.h"

class Demuxer {
    AVFormatContext *fmt_ctx_;
    std::string device_name_;
    AVStream *video_stream_;
    AVStream *audio_stream_;

public:
    Demuxer(const std::string &fmt_name, const std::string &device_name, std::map<std::string, std::string> options);

    ~Demuxer();

    const AVStream *getVideoStream();

    const AVStream *getAudioStream();

    /**
     * Allocate a packet and fill it with the information read from the input format
     * The owneship of the packet is transfered to the caller who will have to free it using av_packet_free
     */
    AVPacket *readPacket();

    void dumpInfo();
};