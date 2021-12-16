#include "encoder.h"

#include <stdexcept>

static void throw_error(const std::string &msg) { throw std::runtime_error("Encoder: " + msg); }

Encoder::Encoder(AVCodecID codec_id) : codec_(nullptr), codec_ctx_(nullptr) {
#ifdef MACOS
    // if (codec_id == AV_CODEC_ID_H264) codec_ = avcodec_find_encoder_by_name("h264_videotoolbox");
#endif
    if (!codec_) codec_ = avcodec_find_encoder(codec_id);
    if (!codec_) throw_error("cannot find codec");

    codec_ctx_ = av::CodecContextUPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throw_error("failed to allocated memory for AVCodecContext");
}

void Encoder::open(const std::map<std::string, std::string> &options) {
    auto dict = av::map2dict(options).release();
    int ret = avcodec_open2(codec_ctx_.get(), codec_, dict ? &dict : nullptr);
    if (dict) av_dict_free(&dict);
    if (ret) throw_error("failed to initialize Codec Context");
}

bool Encoder::sendFrame(const AVFrame *frame) const {
    int ret = avcodec_send_frame(codec_ctx_.get(), frame);
    if (ret == AVERROR(EAGAIN)) {
        return false;
    } else if (ret == AVERROR_EOF) {
        throw std::runtime_error("has already been flushed");
    } else if (ret < 0) {
        throw std::runtime_error("failed to send frame to encoder");
    }
    return true;
}

av::PacketUPtr Encoder::getPacket() const {
    av::PacketUPtr packet(av_packet_alloc());
    if (!packet) throw_error("failed to allocate packet");

    int ret = avcodec_receive_packet(codec_ctx_.get(), packet.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        throw_error("failed to receive frame from decoder");
    }

    return packet;
}

const AVCodecContext *Encoder::getCodecContext() const { return codec_ctx_.get(); }