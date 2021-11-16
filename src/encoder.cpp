#include "../include/encoder.h"

Encoder::Encoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int global_header_flags)
    : codec_(nullptr), codec_ctx_(nullptr), options_(nullptr) {
    try {
#ifdef MACOS
        if (codec_id == AV_CODEC_ID_H264) codec_ = avcodec_find_encoder_by_name("h264_videotoolbox");
#endif
        if (!codec_) codec_ = avcodec_find_encoder(codec_id);
        if (!codec_) throw std::runtime_error("Encoder: Cannot find codec");

        codec_ctx_ = avcodec_alloc_context3(codec_);
        if (!codec_ctx_) throw std::runtime_error("Encoder: Failed to allocated memory for AVCodecContext");

        for (auto const &[key, val] : options) {
            if (av_dict_set(&options_, key.c_str(), val.c_str(), 0) < 0) {
                throw std::runtime_error("Encoder: Cannot set " + key + "in dictionary");
            }
        }

    } catch (const std::exception &e) {
        cleanup();
        throw;
    }
}

Encoder::~Encoder() { cleanup(); }

void Encoder::cleanup() {
    // TO-DO: free codec_ (how?)
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
    if (options_) av_dict_free(&options_);
}

bool Encoder::sendFrame(std::shared_ptr<const AVFrame> frame) const {
    int ret = avcodec_send_frame(codec_ctx_, frame.get());
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
    std::shared_ptr<AVPacket> packet(av_packet_alloc(), Deleter<av_packet_free>());
    if (!packet) throw std::runtime_error("Encoder: failed to allocate packet");

    int ret = avcodec_receive_packet(codec_ctx_, packet.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        throw std::runtime_error("Encoder: Failed to receive frame from decoder");
    }
    return packet;
}

const AVCodecContext *Encoder::getCodecContext() const { return codec_ctx_; }