#include "video_converter.h"

#include <sstream>
#include <stdexcept>

static void throw_error(const std::string &msg) { throw std::runtime_error("Video Converter: " + msg); }

VideoConverter::VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx, int offset_x,
                               int offset_y)
    : buffersrc_ctx_(nullptr), buffersink_ctx_(nullptr) {
    if (!in_codec_ctx) throw_error("in_codec_ctx is NULL");
    if (!out_codec_ctx) throw_error("out_codec_ctx is NULL");

    filter_graph_ = av::FilterGraphUPtr(avfilter_graph_alloc());
    if (!filter_graph_) throw_error("failed to allocate filter graph");

    {
        /* buffer src set-up*/
        std::stringstream args_ss;
        args_ss << "video_size=" << in_codec_ctx->width << "x" << in_codec_ctx->height;
        args_ss << ":pix_fmt=" << in_codec_ctx->pix_fmt;
        args_ss << ":time_base=" << out_codec_ctx->time_base.num << "/" << out_codec_ctx->time_base.den;
        args_ss << ":pixel_aspect=" << in_codec_ctx->sample_aspect_ratio.num << "/"
                << in_codec_ctx->sample_aspect_ratio.den;

        const AVFilter *filter = avfilter_get_by_name("buffer");
        if (!filter) throw_error("failed to find src filter definition");
        if (avfilter_graph_create_filter(&buffersrc_ctx_, filter, "in", args_ss.str().c_str(), nullptr,
                                         filter_graph_.get()) < 0)
            throw_error("failed to create src filter");
    }

    {
        /* buffer sink set-up */
        const AVFilter *filter = avfilter_get_by_name("buffersink");
        if (!filter) throw_error("failed to find sink filter definition");
        if (avfilter_graph_create_filter(&buffersink_ctx_, filter, "out", nullptr, nullptr, filter_graph_.get()) < 0)
            throw_error("failed to create sink filter");
    }

    {
        /* Endpoints for the filter graph. */
        av::FilterInOutUPtr outputs(avfilter_inout_alloc());
        if (!outputs) throw_error("failed to allocate filter outputs");
        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx_;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        av::FilterInOutUPtr inputs(avfilter_inout_alloc());
        if (!inputs) throw_error("failed to allocate filter inputs");
        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx_;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        std::stringstream filter_spec_ss;
        filter_spec_ss << "format=" << out_codec_ctx->pix_fmt;
        filter_spec_ss << ",";
        filter_spec_ss << "crop=" << out_codec_ctx->width << ":" << out_codec_ctx->height << ":" << offset_x << ":"
                       << offset_y;

        AVFilterInOut *outputs_raw = outputs.release();
        AVFilterInOut *inputs_raw = inputs.release();
        int ret = avfilter_graph_parse_ptr(filter_graph_.get(), filter_spec_ss.str().c_str(), &inputs_raw, &outputs_raw,
                                           nullptr);
        outputs = av::FilterInOutUPtr(outputs_raw);
        inputs = av::FilterInOutUPtr(inputs_raw);
        if (ret) throw_error("failed to parse pointers");
    }

    if (avfilter_graph_config(filter_graph_.get(), nullptr) < 0) throw_error("failed to configure the filter graph");
}

bool VideoConverter::sendFrame(const AVFrame *frame) const {
    if (!frame) throw_error("sent frame is not allocated");
    if (av_buffersrc_write_frame(buffersrc_ctx_, frame)) throw_error("failed to write frame to filter");
    return true;
}

av::FrameUPtr VideoConverter::getFrame(int64_t frame_number) const {
    av::FrameUPtr frame(av_frame_alloc());
    if (!frame) throw_error("failed to allocate frame");

    int ret = av_buffersink_get_frame(buffersink_ctx_, frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throw_error("failed to receive frame from filter");

    frame->pts = frame_number;

    return frame;
};