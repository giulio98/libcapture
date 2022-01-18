#include "converter.h"

#include <sstream>

static void throwRuntimeError(const std::string &msg) { throw std::runtime_error("Converter: " + msg); }

void swap(Converter &lhs, Converter &rhs) {
    std::swap(lhs.filter_graph_, rhs.filter_graph_);
    std::swap(lhs.buffersrc_ctx_, rhs.buffersrc_ctx_);
    std::swap(lhs.buffersink_ctx_, rhs.buffersink_ctx_);
    std::swap(lhs.frame_, rhs.frame_);
}

static std::pair<std::string, std::string> getAudioFilterSpec(const AVCodecContext *dec_ctx,
                                                              const AVCodecContext *enc_ctx, AVRational in_time_base) {
    std::stringstream src_args_ss;
    src_args_ss << "time_base=" << in_time_base.num << "/" << in_time_base.den;
    src_args_ss << ":sample_rate=" << dec_ctx->sample_rate;
    src_args_ss << ":sample_fmt=" << dec_ctx->sample_fmt;
    if (dec_ctx->channel_layout) { /* if a specific channel layout is known to the decoder, use it */
        src_args_ss << ":channel_layout=" << dec_ctx->channel_layout;
    } else { /* otherwise, just set the number of channels */
        src_args_ss << ":channels=" << dec_ctx->channels;
    }

    std::stringstream filter_spec_ss;
    /* set PTS */
    filter_spec_ss << "asetpts=PTS-STARTPTS";
    /*
     * convert sample_rate, sample_fmt and channel layout
     * the async=1 option tells the filter to add extra silence / stretch / squeeze the audio
     * to ensure that the timestamps are correct
     */
    filter_spec_ss << ",aresample=" << enc_ctx->sample_rate << ":async=1"
                   << ":out_sample_fmt=" << enc_ctx->sample_fmt << ":out_channel_layout=" << enc_ctx->channel_layout;
    /* ensure correct number of samples for output frames (even with injected silence) */
    filter_spec_ss << ",asetnsamples=n=" << enc_ctx->frame_size;

    /* format conversion (OLD, now use aresample instead) */
    // filter_spec_ss << ",aformat=sample_fmts=" << av_get_sample_fmt_name(enc_ctx->sample_fmt)
    //                << ":sample_rates=" << enc_ctx->sample_rate << ":channel_layouts=" << enc_ctx->channel_layout;

    return std::make_pair(src_args_ss.str(), filter_spec_ss.str());
}

static std::pair<std::string, std::string> getVideoFilterSpec(const AVCodecContext *dec_ctx,
                                                              const AVCodecContext *enc_ctx, AVRational in_time_base,
                                                              int offset_x, int offset_y) {
    std::stringstream src_args_ss;
    src_args_ss << "video_size=" << dec_ctx->width << "x" << dec_ctx->height;
    src_args_ss << ":pix_fmt=" << dec_ctx->pix_fmt;
    src_args_ss << ":time_base=" << in_time_base.num << "/" << in_time_base.den;
    src_args_ss << ":pixel_aspect=" << dec_ctx->sample_aspect_ratio.num << "/" << dec_ctx->sample_aspect_ratio.den;

    std::stringstream filter_spec_ss;
    /* set PTS */
    filter_spec_ss << "setpts=PTS-STARTPTS";
    /* format conversion */
    filter_spec_ss << ",format=" << enc_ctx->pix_fmt;
    /* cropping */
    filter_spec_ss << ",crop=" << enc_ctx->width << ":" << enc_ctx->height << ":" << offset_x << ":" << offset_y;

    return std::make_pair(src_args_ss.str(), filter_spec_ss.str());
}

Converter::Converter(const AVCodecContext *dec_ctx, const AVCodecContext *enc_ctx, AVRational in_time_base,
                     int offset_x, int offset_y) {
    if (!dec_ctx) throwRuntimeError("dec_ctx is NULL");
    if (!enc_ctx) throwRuntimeError("enc_ctx is NULL");

    std::string src_filter_name;
    std::string sink_filter_name;
    std::string src_args;
    std::string filter_spec;

    if (dec_ctx->codec_type != enc_ctx->codec_type) {
        throwRuntimeError("type mismatch between received decoder and encoder");
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        src_filter_name = "buffer";
        sink_filter_name = "buffersink";
        std::tie(src_args, filter_spec) = getVideoFilterSpec(dec_ctx, enc_ctx, in_time_base, offset_x, offset_y);
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (offset_x || offset_y) throwRuntimeError("video offset specified specified for audio converter constructor");
        src_filter_name = "abuffer";
        sink_filter_name = "abuffersink";
        std::tie(src_args, filter_spec) = getAudioFilterSpec(dec_ctx, enc_ctx, in_time_base);
    } else {
        throwRuntimeError("unknown media type received in constructor");
    }

    filter_graph_ = av::FilterGraphUPtr(avfilter_graph_alloc());
    if (!filter_graph_) throwRuntimeError("failed to allocate filter graph");

    { /* buffer src set-up*/
        const AVFilter *filter = avfilter_get_by_name(src_filter_name.c_str());
        if (!filter) throwRuntimeError("failed to find src filter definition");
        if (avfilter_graph_create_filter(&buffersrc_ctx_, filter, "in", src_args.c_str(), nullptr,
                                         filter_graph_.get()) < 0)
            throwRuntimeError("failed to create src filter");
    }

    { /* buffer sink set-up*/
        const AVFilter *filter = avfilter_get_by_name(sink_filter_name.c_str());
        if (!filter) throwRuntimeError("failed to find sink filter definition");
        if (avfilter_graph_create_filter(&buffersink_ctx_, filter, "out", nullptr, nullptr, filter_graph_.get()) < 0)
            throwRuntimeError("failed to create src filter");
    }

    {
        /* Endpoints for the filter graph. */
        av::FilterInOutUPtr outputs(avfilter_inout_alloc());
        if (!outputs) throwRuntimeError("failed to allocate filter outputs");
        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx_;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        av::FilterInOutUPtr inputs(avfilter_inout_alloc());
        if (!inputs) throwRuntimeError("failed to allocate filter inputs");
        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx_;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        AVFilterInOut *outputs_raw = outputs.release();
        AVFilterInOut *inputs_raw = inputs.release();
        int ret =
            avfilter_graph_parse_ptr(filter_graph_.get(), filter_spec.c_str(), &inputs_raw, &outputs_raw, nullptr);
        avfilter_inout_free(&outputs_raw);
        avfilter_inout_free(&inputs_raw);
        if (ret < 0) throwRuntimeError("failed to parse pointers");
    }

    if (avfilter_graph_config(filter_graph_.get(), nullptr) < 0)
        throwRuntimeError("failed to configure the filter graph");
}

Converter::Converter(Converter &&other) noexcept { swap(*this, other); }

Converter &Converter::operator=(Converter other) {
    swap(*this, other);
    return *this;
}

void Converter::sendFrame(av::FrameUPtr frame) {
    if (!buffersrc_ctx_) throwRuntimeError("buffersrc is not allocated");
    if (!frame) throwRuntimeError("sent frame is not allocated");
    if (av_buffersrc_add_frame(buffersrc_ctx_, frame.get())) throwRuntimeError("failed to write frame to filter");
}

av::FrameUPtr Converter::getFrame() {
    if (!buffersink_ctx_) throwRuntimeError("buffersink is not allocated");

    if (!frame_) {
        frame_ = av::FrameUPtr(av_frame_alloc());
        if (!frame_) throwRuntimeError("failed to allocate frame");
    }

    int ret = av_buffersink_get_frame(buffersink_ctx_, frame_.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throwRuntimeError("failed to receive frame from filter");

    return std::move(frame_);
}