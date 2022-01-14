#include "encoder.h"

#include <iostream>
#include <stdexcept>

#define VERBOSE 0

static void throw_error(const std::string &msg) { throw std::runtime_error("Encoder: " + msg); }

Encoder::Encoder(AVCodecID codec_id) {
#ifdef MACOS
    // if (codec_id == AV_CODEC_ID_H264) {
    //     codec_ = avcodec_find_encoder_by_name("h264_videotoolbox");
    // } else if (codec_id == AV_CODEC_ID_AAC) {
    //     codec_ = avcodec_find_encoder_by_name("aac_at");
    // }
#endif

    if (!codec_) {
        codec_ = avcodec_find_encoder(codec_id);
        if (!codec_) throw_error("cannot find codec");
    }

    codec_ctx_ = av::CodecContextUPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throw_error("failed to allocated memory for AVCodecContext");
}

Encoder::Encoder(AVCodecID codec_id, int sample_rate, uint64_t channel_layout, int global_header_flags,
                 const std::map<std::string, std::string> &options)
    : Encoder(codec_id) {
    codec_ctx_->sample_rate = sample_rate;
    codec_ctx_->channel_layout = channel_layout;
    codec_ctx_->channels = av_get_channel_layout_nb_channels(codec_ctx_->channel_layout);
    if (codec_->sample_fmts) codec_ctx_->sample_fmt = codec_->sample_fmts[0];
    /* for audio, the time base will be automatically set by init() */
    // codec_ctx_->time_base.num = 1;
    // codec_ctx_->time_base.den = codec_ctx_->sample_rate;

    init(global_header_flags, options);
}

Encoder::Encoder(AVCodecID codec_id, int width, int height, AVPixelFormat pix_fmt, AVRational time_base,
                 int global_header_flags, const std::map<std::string, std::string> &options)
    : Encoder(codec_id) {
    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->pix_fmt = pix_fmt;
    codec_ctx_->time_base = time_base;

    init(global_header_flags, options);
}

Encoder::Encoder(Encoder &&other) {
    codec_ = other.codec_;
    other.codec_ = nullptr;
    codec_ctx_ = std::move(other.codec_ctx_);
    packet_ = std::move(other.packet_);
}

Encoder &Encoder::operator=(Encoder &&other) {
    if (this != &other) {
        codec_ = other.codec_;
        other.codec_ = nullptr;
        codec_ctx_ = std::move(other.codec_ctx_);
        packet_ = std::move(other.packet_);
    }
    return *this;
}

void Encoder::init(int global_header_flags, const std::map<std::string, std::string> &options) {
    if (!codec_) throw_error("initialization failed, internal codec is null");
    if (!codec_ctx_) throw_error("initialization failed, internal codec ctx is null");

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av::DictionaryUPtr dict = av::map2dict(options);
    AVDictionary *dict_raw = dict.release();
    int ret = avcodec_open2(codec_ctx_.get(), codec_, dict_raw ? &dict_raw : nullptr);
    dict = av::DictionaryUPtr(dict_raw);
    if (ret) throw_error("failed to initialize Codec Context");
#if VERBOSE
    auto map = av::dict2map(dict.get());
    for (const auto &[key, val] : map) {
        std::cerr << "Encoder: couldn't find any '" << key << "' option" << std::endl;
    }
#endif
}

bool Encoder::sendFrame(const AVFrame *frame) {
    if (!codec_ctx_) throw_error("encoder was not initialized yet");
    int ret = avcodec_send_frame(codec_ctx_.get(), frame);
    if (ret == AVERROR(EAGAIN)) return false;
    if (ret == AVERROR_EOF) throw_error("has already been flushed");
    if (ret < 0) throw_error("failed to send frame to encoder");
    return true;
}

av::PacketUPtr Encoder::getPacket() {
    if (!codec_ctx_) throw_error("encoder was not initialized yet");

    if (!packet_) {
        packet_ = av::PacketUPtr(av_packet_alloc());
        if (!packet_) throw_error("failed to allocate packet");
    }

    int ret = avcodec_receive_packet(codec_ctx_.get(), packet_.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throw_error("failed to receive frame from decoder");

    return std::move(packet_);
}

const AVCodecContext *Encoder::getContext() const { return codec_ctx_.get(); }

std::string Encoder::getName() const {
    if (codec_) return codec_->long_name;
    return std::string{};
}