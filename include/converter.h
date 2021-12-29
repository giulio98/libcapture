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
    virtual void sendFrame(av::FrameUPtr frame) const = 0;

    /**
     * Get a converted frame
     * @param frame_number the frame's sequence number to use to compute the PTS
     * @return a new converted frame if it was possible to build it, nullptr otherwise
     */
    [[nodiscard]] virtual av::FrameUPtr getFrame() const = 0;
};