#pragma once

#include <map>
#include <string>

#include "decoder.h"
#include "deleter.h"
#include "ffmpeg_libs.h"

class Demuxer {
    using unique_ptr_fmt_ctx = std::unique_ptr<AVFormatContext, DeleterPP<avformat_close_input>>;

    unique_ptr_fmt_ctx fmt_ctx_;
    std::string device_name_;
    const AVStream *video_stream_;
    const AVStream *audio_stream_;

public:
    Demuxer(const std::string &fmt_name, const std::string &device_name,
            const std::map<std::string, std::string> &options);

    ~Demuxer();

    const AVCodecParameters *getVideoParams() const;

    const AVCodecParameters *getAudioParams() const;

    /**
     * Read a packet from the input device and return it together with its type
     * @return a packet and its type if it was possible to read it, nullptr if the demuxer had nothing to read
     */
    std::pair<std::shared_ptr<const AVPacket>, AVType> getPacket() const;

    void dumpInfo() const;
};