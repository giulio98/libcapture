#include "encoder.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

#define VERBOSE 0  // TO-DO: improve

static std::string errMsg(const std::string &msg) { return ("Encoder: " + msg); }

void swap(Encoder &lhs, Encoder &rhs) {
    std::swap(lhs.codec_, rhs.codec_);
    std::swap(lhs.codec_ctx_, rhs.codec_ctx_);
    std::swap(lhs.packet_, rhs.packet_);
}

Encoder::Encoder(const AVCodecID codec_id) {
#ifdef MACOS
    // if (codec_id == AV_CODEC_ID_H264) {
    //     codec_ = avcodec_find_encoder_by_name("h264_videotoolbox");
    // } else if (codec_id == AV_CODEC_ID_AAC) {
    //     codec_ = avcodec_find_encoder_by_name("aac_at");
    // }
#endif

    if (!codec_) {
        codec_ = avcodec_find_encoder(codec_id);
        if (!codec_) throw std::runtime_error(errMsg("cannot find encoder"));
    }

    codec_ctx_ = av::CodecContextUPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throw std::runtime_error(errMsg("failed to allocated memory for AVCodecContext"));
}

Encoder::Encoder(const AVCodecID codec_id, const int sample_rate, const uint64_t channel_layout,
                 const int global_header_flags, const std::map<std::string, std::string> &options)
    : Encoder(codec_id) {
    if (codec_->type != AVMEDIA_TYPE_AUDIO)
        throw std::invalid_argument(errMsg("failed to create audio encoder (received codec ID is not of type audio)"));

    codec_ctx_->sample_rate = sample_rate;
    codec_ctx_->channel_layout = channel_layout;
    codec_ctx_->channels = av_get_channel_layout_nb_channels(codec_ctx_->channel_layout);
    if (codec_->sample_fmts) codec_ctx_->sample_fmt = codec_->sample_fmts[0];
    /* for audio, the time base will be automatically set by init() */
    // codec_ctx_->time_base.num = 1;
    // codec_ctx_->time_base.den = codec_ctx_->sample_rate;

    init(global_header_flags, options);
}

Encoder::Encoder(const AVCodecID codec_id, const int width, const int height, const AVPixelFormat pix_fmt,
                 const AVRational time_base, const int global_header_flags,
                 const std::map<std::string, std::string> &options)
    : Encoder(codec_id) {
    if (codec_->type != AVMEDIA_TYPE_VIDEO)
        throw std::invalid_argument(errMsg("failed to create video encoder (received codec ID is not of type video)"));

    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->pix_fmt = pix_fmt;
    codec_ctx_->time_base = time_base;

    init(global_header_flags, options);
}

Encoder::Encoder(Encoder &&other) noexcept { swap(*this, other); }

Encoder &Encoder::operator=(Encoder other) {
    swap(*this, other);
    return *this;
}

void Encoder::init(const int global_header_flags, const std::map<std::string, std::string> &options) {
    assert(codec_);
    assert(codec_ctx_);

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av::DictionaryUPtr dict = av::map2dict(options);
    AVDictionary *dict_raw = dict.release();
    int ret = avcodec_open2(codec_ctx_.get(), codec_, dict_raw ? &dict_raw : nullptr);
    dict = av::DictionaryUPtr(dict_raw);
    if (ret) throw std::logic_error(errMsg("failed to initialize Codec Context"));
#if VERBOSE
    auto map = av::dict2map(dict.get());
    for (const auto &[key, val] : map) {
        std::cerr << "Encoder: couldn't find any '" << key << "' option" << std::endl;
    }
#endif
}

bool Encoder::sendFrame(const AVFrame *frame) {
    if (!codec_ctx_) throw std::logic_error(errMsg("encoder was not initialized yet"));
    int ret = avcodec_send_frame(codec_ctx_.get(), frame);
    if (ret == AVERROR(EAGAIN)) return false;
    if (ret == AVERROR_EOF) throw std::logic_error(errMsg("has already been flushed"));
    if (ret < 0) throw std::runtime_error(errMsg("failed to send frame to encoder"));
    return true;
}

av::PacketUPtr Encoder::getPacket() {
    if (!codec_ctx_) throw std::logic_error(errMsg("encoder was not initialized yet"));

    if (!packet_) {
        packet_ = av::PacketUPtr(av_packet_alloc());
        if (!packet_) throw std::runtime_error(errMsg("failed to allocate packet"));
    }

    int ret = avcodec_receive_packet(codec_ctx_.get(), packet_.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throw std::runtime_error(errMsg("failed to receive frame from decoder"));

    return std::move(packet_);
}

const AVCodecContext *Encoder::getContext() const { return codec_ctx_.get(); }

std::string Encoder::getName() const {
    if (codec_) return codec_->long_name;
    return std::string{};
}