#pragma once

#include <array>
#include <map>
#include <string>

#include "common/common.h"

class Demuxer {
    av::InFormatContextUPtr fmt_ctx_;
    AVInputFormat *fmt_ = nullptr;
    std::string device_name_;
    std::map<std::string, std::string> options_;
    std::array<const AVStream *, av::DataType::NumTypes> streams_ = {nullptr, nullptr};
    av::PacketUPtr packet_;

public:
    Demuxer() = default;

    /**
     * Create a new demuxer (whose input device will be in a "closed" state)
     * @param fmt_name      the name of the input format
     * @param device_name   the name of the device to open
     * @param options       a map containing the options to use when opening the input device
     */
    Demuxer(const std::string &fmt_name, std::string device_name, std::map<std::string, std::string> options);

    Demuxer(const Demuxer &) = delete;

    Demuxer(Demuxer &&other);

    ~Demuxer() = default;

    Demuxer &operator=(const Demuxer &) = delete;

    Demuxer &operator=(Demuxer &&other);

    /**
     * Open the input device
     * WARNING: calling this function when the input is already open will throw an exception
     */
    void openInput();

    /**
     * Close the input device
     * WARNING: calling this function when the input is already closed will throw an exception
     */
    void closeInput();

    /**
     * Flush the internal buffered data
     * WARNING: calling this function when the input is closed will throw an exception
     */
    void flush();

    /**
     * Check whether the input managed by the demuxer is open or not
     * @return true if the input is open, false otherwise
     */
    [[nodiscard]] bool isInputOpen() const;

    /**
     * Access the stream parameters
     * @param stream_type the type of data of the stream
     * @return an observer pointer to access the stream parameters
     */
    [[nodiscard]] const AVCodecParameters *getStreamParams(av::DataType stream_type) const;

    /**
     * Get the time-base of the stream
     * @param stream_type the type of data of the stream
     * @return the stream time-base
     */
    [[nodiscard]] AVRational getStreamTimeBase(av::DataType stream_type) const;

    /**
     * Read a packet from the input device and return it together with its type
     * @return a packet and its type if it was possible to read it, nullptr and a random meaningless type
     * if there was nothing to read
     */
    std::pair<av::PacketUPtr, av::DataType> readPacket();

    /**
     * Print informations about the streams
     * @param index the index to print for this device
     */
    void printInfo(int index = 0) const;
};