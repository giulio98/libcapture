#pragma once

#include <map>
#include <string>

#include "common.h"

class Demuxer {
    av::InFormatContextUPtr fmt_ctx_;
    AVInputFormat *fmt_;
    std::string device_name_;
    std::map<std::string, std::string> options_;
    const AVStream *video_stream_;
    const AVStream *audio_stream_;

public:
    Demuxer(const std::string &fmt_name, const std::string &device_name,
            const std::map<std::string, std::string> &options);

    void openInput();

    void closeInput();

    const AVCodecParameters *getVideoParams() const;

    const AVCodecParameters *getAudioParams() const;

    /**
     * Read a packet from the input device and return it together with its type
     * @return a packet and its type if it was possible to read it, nullptr if the demuxer had nothing to read
     */
    std::pair<av::PacketUPtr, av::DataType> readPacket() const;

    void flush() const;

    void dumpInfo(int index) const;
};