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
    /**
     * Create a new demuxer
     * @param fmt_name      the name of the input format
     * @param device_name   the name of the device to open
     * @param options       a map containing the options to use when opening the input device
     */
    Demuxer(const std::string &fmt_name, std::string device_name, std::map<std::string, std::string> options);

    /**
     * Open the input device
     */
    void openInput();

    /**
     * Close the input device
     */
    void closeInput();

    /**
     * Access the video stream parameters
     * @return an observer pointer to access the parameters
     */
    [[nodiscard]] const AVCodecParameters *getVideoParams() const;

    /**
     * Access the audio stream parameters
     * @return an observer pointer to access the parameters
     */
    [[nodiscard]] const AVCodecParameters *getAudioParams() const;

    /**
     * Read a packet from the input device and return it together with its type
     * @return a packet and its type if it was possible to read it, nullptr and av::DataType::none if the demuxer had
     * nothing to read
     */
    [[nodiscard]] std::pair<av::PacketUPtr, av::DataType> readPacket() const;

    /**
     * Print informations about the streams
     * @param index the index to print for this device
     */
    void dumpInfo(int index = 0) const;
};