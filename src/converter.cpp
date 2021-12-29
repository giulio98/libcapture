#include "converter.h"

void Converter::throwError(const std::string &msg) const { throw std::runtime_error("Converter: " + msg); }

Converter::Converter() : buffersrc_ctx_(nullptr), buffersink_ctx_(nullptr) {
    filter_graph_ = av::FilterGraphUPtr(avfilter_graph_alloc());
    if (!filter_graph_) throwError("failed to allocate filter graph");
}

void Converter::init(const std::string &src_name, const std::string &sink_name, const std::string &src_args,
                     const std::string &filter_spec) {
    { /* buffer src set-up*/
        const AVFilter *filter = avfilter_get_by_name(src_name.c_str());
        if (!filter) throwError("failed to find src filter definition");
        if (avfilter_graph_create_filter(&buffersrc_ctx_, filter, "in", src_args.c_str(), nullptr,
                                         filter_graph_.get()) < 0)
            throwError("failed to create src filter");
    }

    { /* buffer sink set-up*/
        const AVFilter *filter = avfilter_get_by_name(sink_name.c_str());
        if (!filter) throwError("failed to find sink filter definition");
        if (avfilter_graph_create_filter(&buffersink_ctx_, filter, "out", nullptr, nullptr, filter_graph_.get()) < 0)
            throwError("failed to create src filter");
    }

    {
        /* Endpoints for the filter graph. */
        av::FilterInOutUPtr outputs(avfilter_inout_alloc());
        if (!outputs) throwError("failed to allocate filter outputs");
        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx_;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        av::FilterInOutUPtr inputs(avfilter_inout_alloc());
        if (!inputs) throwError("failed to allocate filter inputs");
        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx_;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        AVFilterInOut *outputs_raw = outputs.release();
        AVFilterInOut *inputs_raw = inputs.release();
        int ret =
            avfilter_graph_parse_ptr(filter_graph_.get(), filter_spec.c_str(), &inputs_raw, &outputs_raw, nullptr);
        outputs = av::FilterInOutUPtr(outputs_raw);
        inputs = av::FilterInOutUPtr(inputs_raw);
        if (ret < 0) throwError("failed to parse pointers");
    }

    if (avfilter_graph_config(filter_graph_.get(), nullptr) < 0) throwError("failed to configure the filter graph");
}

void Converter::sendFrame(av::FrameUPtr frame) const {
    if (!buffersrc_ctx_) throwError("buffersrc is not allocated");
    if (!frame) throwError("sent frame is not allocated");
    if (av_buffersrc_add_frame(buffersrc_ctx_, frame.get())) throwError("failed to write frame to filter");
}

av::FrameUPtr Converter::getFrame() const {
    if (!buffersink_ctx_) throwError("buffersink is not allocated");

    av::FrameUPtr frame(av_frame_alloc());
    if (!frame) throwError("failed to allocate frame");

    int ret = av_buffersink_get_frame(buffersink_ctx_, frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throwError("failed to receive frame from filter");

    return frame;
};