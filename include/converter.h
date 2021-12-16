#pragma once

#include "common.h"

class Converter {
public:
    Converter() = default;

    virtual ~Converter() = default;

    /**
     * Send a frame to convert
     * @return true if the conversion was successful, false otherwise
     */
    virtual bool sendFrame(const AVFrame *frame) const = 0;

    /**
     * Get a converted frame
     * @param frame_number the sequence number of the frame to use to compute the PTS
     * @return a new converted frame if it was possible to build it, nullptr otherwise
     */
    [[nodiscard]] virtual av::FrameUPtr getFrame(int64_t frame_number = 0) const = 0;
};