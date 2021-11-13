#include "../include/encoder.h"

Encoder::Encoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int global_header_flags)
    : codec_(nullptr), codec_ctx_(nullptr), options_(nullptr) {
    codec_ = avcodec_find_encoder(codec_id);
    if (!codec_) throw std::runtime_error("Encoder: Cannot find codec");

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) throw std::runtime_error("Encoder: Failed to allocated memory for AVCodecContext");

    for (auto const &[key, val] : options) {
        if (av_dict_set(&options_, key.c_str(), val.c_str(), 0) < 0) {
            throw std::runtime_error("Encoder: Cannot set " + key + "in dictionary");
        }
    }
}

Encoder::~Encoder() {
    // TO-DO: free codec_ (how?)
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
    if (options_) av_dict_free(&options_);
}

bool Encoder::sendFrame(const AVFrame *frame) {
    int ret = avcodec_send_frame(codec_ctx_, frame);
    if (ret == AVERROR(EAGAIN)) {
        return false;
    } else if (ret == AVERROR_EOF) {
        throw FlushedException("Encoder");
    } else if (ret < 0) {
        throw std::runtime_error("Encoder: Failed to send frame to encoder");
    }
    return true;
}

bool Encoder::fillPacket(AVPacket *packet) {
    if (!packet) throw std::runtime_error("Encoder: Packet is not allocated");

    int ret = avcodec_receive_packet(codec_ctx_, packet);
    if (ret == AVERROR(EAGAIN)) {
        return false;
    } else if (ret == AVERROR_EOF) {
        throw FlushedException("Encoder");
    } else if (ret < 0) {
        throw std::runtime_error("Encoder: Failed to receive frame from decoder");
    }
    return true;
}

const AVCodecContext *Encoder::getCodecContext() { return codec_ctx_; }