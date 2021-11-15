#pragma once

#include <map>
#include <string>

#include "decoder.h"
#include "deleters.h"
#include "ffmpeg_libs.h"

class Demuxer {
    AVFormatContext *fmt_ctx_;
    std::string device_name_;
    AVStream *video_stream_;
    AVStream *audio_stream_;

    void cleanup();

public:
    Demuxer(const std::string &fmt_name, const std::string &device_name,
            const std::map<std::string, std::string> &options);

    ~Demuxer();

    int getVideoStreamIdx() const;
    int getAudioStreamIdx() const;

    const AVCodecParameters *getVideoParams() const;
    const AVCodecParameters *getAudioParams() const;

    /**
     * Read a packet from the input device and return it
     * @return a packet if it was possible to read it, nullptr if the demuxer had nothing to read
     */
    std::shared_ptr<const AVPacket> getPacket() const;

    void dumpInfo() const;
};