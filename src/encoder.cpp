#include "../include/encoder.h"

Encoder::Encoder(AVCodecID codec_id) : codec_(nullptr), codec_ctx_(nullptr) {
#ifdef MACOS
    if (codec_id == AV_CODEC_ID_H264) codec_ = avcodec_find_encoder_by_name("h264_videotoolbox");
#endif
    if (!codec_) codec_ = avcodec_find_encoder(codec_id);
    if (!codec_) throw std::runtime_error("Encoder: Cannot find codec");

    codec_ctx_ = av::CodecContextPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throw std::runtime_error("Encoder: Failed to allocated memory for AVCodecContext");
}

void Encoder::open(const std::map<std::string, std::string> &options) {
    auto dict = av::map2dict(options).release();
    int err = avcodec_open2(codec_ctx_.get(), codec_, dict ? &dict : nullptr);
    av_dict_free(&dict);
    if (err) throw std::runtime_error("Encoder: Failed to initialize Codec Context");
}

bool Encoder::sendFrame(std::shared_ptr<const AVFrame> frame) const {
    int ret = avcodec_send_frame(codec_ctx_.get(), frame.get());
    if (ret == AVERROR(EAGAIN)) {
        return false;
    } else if (ret == AVERROR_EOF) {
        throw std::runtime_error("Encoder has already been flushed");
    } else if (ret < 0) {
        throw std::runtime_error("Failed to send frame to encoder");
    }
    return true;
}

std::shared_ptr<AVPacket> Encoder::getPacket() const {
    std::shared_ptr<AVPacket> packet(av_packet_alloc(), DeleterPP<av_packet_free>());
    if (!packet) throw std::runtime_error("Encoder: failed to allocate packet");

    int ret = avcodec_receive_packet(codec_ctx_.get(), packet.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        throw std::runtime_error("Encoder: Failed to receive frame from decoder");
    }
    return packet;
}

const AVCodecContext *Encoder::getCodecContext() const { return codec_ctx_.get(); }