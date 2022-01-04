#include "encoder.h"

#include <stdexcept>

#define VERBOSE 0

static void throw_error(const std::string &msg) { throw std::runtime_error("Encoder: " + msg); }

Encoder::Encoder(AVCodecID codec_id) : codec_(nullptr) {
#ifdef MACOS
    if (codec_id == AV_CODEC_ID_H264) {
        codec_ = avcodec_find_encoder_by_name("h264_videotoolbox");
    } else if (codec_id == AV_CODEC_ID_AAC) {
        codec_ = avcodec_find_encoder_by_name("aac_at");
    }
#endif
    if (!codec_) codec_ = avcodec_find_encoder(codec_id);
    if (!codec_) throw_error("cannot find codec");

    codec_ctx_ = av::CodecContextUPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throw_error("failed to allocated memory for AVCodecContext");
}

const AVCodec *Encoder::getCodec() const { return codec_; }

AVCodecContext *Encoder::getCodecContextMod() const { return codec_ctx_.get(); }

void Encoder::init(const std::map<std::string, std::string> &options) {
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

bool Encoder::sendFrame(const AVFrame *frame) const {
    int ret = avcodec_send_frame(codec_ctx_.get(), frame);
    if (ret == AVERROR(EAGAIN)) return false;
    if (ret == AVERROR_EOF) throw_error("has already been flushed");
    if (ret < 0) throw_error("failed to send frame to encoder");
    return true;
}

av::PacketUPtr Encoder::getPacket() const {
    av::PacketUPtr packet(av_packet_alloc());
    if (!packet) throw_error("failed to allocate packet");

    int ret = avcodec_receive_packet(codec_ctx_.get(), packet.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throw_error("failed to receive frame from decoder");

    return packet;
}

const AVCodecContext *Encoder::getCodecContext() const { return codec_ctx_.get(); }

std::string Encoder::getName() const { return codec_->name; }