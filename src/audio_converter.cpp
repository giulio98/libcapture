#include "audio_converter.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

static void throw_error(const std::string &msg) { throw std::runtime_error("Audio Converter: " + msg); }

AudioConverter::AudioConverter(const AVCodecContext *in_codec_ctx, const AVCodecContext *out_codec_ctx)
    : buffersrc_ctx_(nullptr), buffersink_ctx_(nullptr) {
    if (!in_codec_ctx) throw_error("in_codec_ctx is NULL");
    if (!out_codec_ctx) throw_error("out_codec_ctx is NULL");

    filter_graph_ = av::FilterGraphUPtr(avfilter_graph_alloc());
    if (!filter_graph_) throw_error("failed to allocate filter graph");

    {
        /* buffer src set-up*/
        std::stringstream args_ss;
        args_ss << "time_base=" << out_codec_ctx->time_base.num << "/" << out_codec_ctx->time_base.den;
        args_ss << ":sample_rate=" << in_codec_ctx->sample_rate;
        args_ss << ":sample_fmt=" << av_get_sample_fmt_name(in_codec_ctx->sample_fmt);
        args_ss << ":channel_layout=" << in_codec_ctx->channel_layout;

        const AVFilter *filter = avfilter_get_by_name("abuffer");
        if (!filter) throw_error("failed to find src filter definition");
        if (avfilter_graph_create_filter(&buffersrc_ctx_, filter, "in", args_ss.str().c_str(), nullptr,
                                         filter_graph_.get()) < 0)
            throw_error("failed to create src filter");
    }

    {
        const AVFilter *filter = avfilter_get_by_name("abuffersink");
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
        filter_spec_ss << "asetnsamples=n=" << out_codec_ctx->frame_size;
        filter_spec_ss << ",";
        filter_spec_ss << "aformat=sample_fmts=" << av_get_sample_fmt_name(out_codec_ctx->sample_fmt)
                       << ":sample_rates=" << out_codec_ctx->sample_rate
                       << ":channel_layouts=" << av_get_channel_name(out_codec_ctx->channel_layout);
        filter_spec_ss << ",";
        filter_spec_ss << "asetpts=PTS-STARTPTS";

        AVFilterInOut *outputs_raw = outputs.release();
        AVFilterInOut *inputs_raw = inputs.release();
        int ret = avfilter_graph_parse_ptr(filter_graph_.get(), filter_spec_ss.str().c_str(), &inputs_raw, &outputs_raw,
                                           nullptr);
        outputs = av::FilterInOutUPtr(outputs_raw);
        inputs = av::FilterInOutUPtr(inputs_raw);
        if (ret < 0) throw_error("failed to parse pointers");
    }

    if (avfilter_graph_config(filter_graph_.get(), nullptr) < 0) throw_error("failed to configure the filter graph");
}

void AudioConverter::sendFrame(av::FrameUPtr frame) const {
    if (!frame) throw_error("sent frame is not allocated");
    if (av_buffersrc_add_frame(buffersrc_ctx_, frame.get())) throw_error("failed to write frame to filter");
}

av::FrameUPtr AudioConverter::getFrame() const {
    av::FrameUPtr frame(av_frame_alloc());
    if (!frame) throw_error("failed to allocate frame");

    int ret = av_buffersink_get_frame(buffersink_ctx_, frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throw_error("failed to receive frame from filter");

    return frame;
}