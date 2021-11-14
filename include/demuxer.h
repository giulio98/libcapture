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
    AVPacket *packet_;

public:
    Demuxer(const std::string &fmt_name, const std::string &device_name,
            const std::map<std::string, std::string> &options);

    ~Demuxer();

    int getVideoStreamIdx() const;
    int getAudioStreamIdx() const;

    const AVCodecParameters *getVideoParams() const;
    const AVCodecParameters *getAudioParams() const;

    /**
     * Fill an allocated packet with the information read from the input format
     * The owneship of the packet remains to the caller
     * @return true if the packet has been correctly filled, false if the demuxer had nothing to write
     */
    const AVPacket *getPacket() const;

    void dumpInfo() const;
};