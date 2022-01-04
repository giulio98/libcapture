#pragma once

#include "common.h"

class Converter {
    av::FilterGraphUPtr filter_graph_;
    AVFilterContext *buffersrc_ctx_;
    AVFilterContext *buffersink_ctx_;
    av::FrameUPtr frame_;

protected:
    Converter();

    void init(const std::string &src_name, const std::string &sink_name, const std::string &src_args,
              const std::string &filter_spec);

    void throwError(const std::string &msg) const;

public:
    ~Converter() = default;

    /**
     * Send a frame to convert
     */
    void sendFrame(av::FrameUPtr frame) const;

    /**
     * Get a converted frame
     * @return a new converted frame if it was possible to build it, nullptr otherwise
     */
    [[nodiscard]] av::FrameUPtr getFrame();
};