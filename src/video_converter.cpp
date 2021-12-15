#include "video_converter.h"

#include <sstream>
#include <stdexcept>

static void throw_error(std::string msg) { throw std::runtime_error("Video Converter: " + msg); }

VideoConverter::VideoConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx, int offset_x,
                               int offset_y)
    : offset_x_(offset_x), offset_y_(offset_y), buffersrc_ctx_(nullptr), buffersink_ctx_(nullptr) {
    if (!in_codec_ctx) throw_error("in_codec_ctx is NULL");
    if (!out_codec_ctx) throw_error("out_codec_ctx is NULL");

    in_width_ = in_codec_ctx->width;
    in_height_ = in_codec_ctx->height;
    out_width_ = out_codec_ctx->width;
    out_height_ = out_codec_ctx->height;
    out_pix_fmt_ = out_codec_ctx->pix_fmt;

    sws_ctx_ = av::SwsContextUPtr(sws_getContext(in_width_, in_height_, in_codec_ctx->pix_fmt, in_width_, in_height_,
                                                 out_pix_fmt_, SWS_BICUBIC, nullptr, nullptr, nullptr));
    if (!sws_ctx_) throw_error("failed to allocate context");

    filter_graph_ = av::FilterGraphUPtr(avfilter_graph_alloc());
    if (!filter_graph_) throw_error("failed to allocate filter graph");

    {
        /* buffer src set-up*/
        std::stringstream args_ss;
        args_ss << "video_size=" << in_width_ << "x" << in_height_;
        args_ss << ":pix_fmt=" << out_pix_fmt_;
        args_ss << ":time_base=" << out_codec_ctx->time_base.num << "/" << out_codec_ctx->time_base.den;
        args_ss << ":pixel_aspect=" << out_codec_ctx->sample_aspect_ratio.num << "/"
                << out_codec_ctx->sample_aspect_ratio.den;

        const AVFilter *buffersrc = avfilter_get_by_name("buffer");
        if (!buffersrc) throw_error("failed to find src filter definition");

        if (avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc, "in", args_ss.str().c_str(), nullptr,
                                         filter_graph_.get()) < 0)
            throw_error("failed to create src filter");
    }

    {
        /* buffer sink set-up */
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");
        if (!buffersink) throw_error("failed to find sink filter definition");
        if (avfilter_graph_create_filter(&buffersink_ctx_, buffersink, "out", nullptr, nullptr, filter_graph_.get()) <
            0)
            throw_error("failed to create sink filter");
    }

    {
        /* Endpoints for the filter graph. */
        AVFilterInOut *inputs = nullptr;
        AVFilterInOut *outputs = nullptr;

        try {
            inputs = avfilter_inout_alloc();
            if (!inputs) throw_error("failed to allocate filter inputs");
            inputs->name = av_strdup("out");
            inputs->filter_ctx = buffersink_ctx_;
            inputs->pad_idx = 0;
            inputs->next = nullptr;

            outputs = avfilter_inout_alloc();
            if (!outputs) throw_error("failed to allocate filter outputs");
            outputs->name = av_strdup("in");
            outputs->filter_ctx = buffersrc_ctx_;
            outputs->pad_idx = 0;
            outputs->next = nullptr;

            std::stringstream filter_name_ss;
            filter_name_ss << "crop=" << out_width_ << ":" << out_height_ << ":" << offset_x_ << ":" << offset_y_;

            if (avfilter_graph_parse_ptr(filter_graph_.get(), filter_name_ss.str().c_str(), &inputs, &outputs,
                                         nullptr) < 0)
                throw_error("failed to parse pointers");

            if (inputs) avfilter_inout_free(&inputs);
            if (outputs) avfilter_inout_free(&outputs);

        } catch (const std::exception &e) {
            if (inputs) avfilter_inout_free(&inputs);
            if (outputs) avfilter_inout_free(&outputs);
            throw;
        }
    }

    if (avfilter_graph_config(filter_graph_.get(), nullptr) < 0) throw_error("failed to configure the filter graph");
}

bool VideoConverter::sendFrame(const AVFrame *in_frame) const {
    if (!in_frame) throw_error("in_frame is not allocated");

    av::FrameUPtr converted_frame(av_frame_alloc());
    if (!converted_frame) throw_error("failed to allocate frame");

    converted_frame->width = in_width_;
    converted_frame->height = in_height_;
    converted_frame->format = out_pix_fmt_;
    if (av_frame_get_buffer(converted_frame.get(), 0)) throw_error("failed to allocate frame data");

    if (sws_scale(sws_ctx_.get(), in_frame->data, in_frame->linesize, 0, in_height_, converted_frame->data,
                  converted_frame->linesize) < 0)
        throw_error("failed to convert frame");

    if (av_buffersrc_add_frame(buffersrc_ctx_, converted_frame.get())) throw_error("failed to add frame to filter");

    return true;
}

av::FrameUPtr VideoConverter::getFrame(int64_t frame_number) const {
    av::FrameUPtr frame(av_frame_alloc());
    if (!frame) throw_error("failed to allocate frame");

    int ret = av_buffersink_get_frame(buffersink_ctx_, frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        throw_error("failed to receive frame from filter");
    }

    frame->pts = frame_number;

    return frame;
};