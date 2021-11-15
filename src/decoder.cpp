#include "../include/decoder.h"

Decoder::Decoder(const AVCodecParameters *params) : codec_(nullptr), codec_ctx_(nullptr) {
    try {
        codec_ = avcodec_find_decoder(params->codec_id);
        if (!codec_) throw std::runtime_error("Decoder: Cannot find codec");

        codec_ctx_ = avcodec_alloc_context3(codec_);
        if (!codec_ctx_) throw std::runtime_error("Decoder: Failed to allocated memory for AVCodecContext");

        if (avcodec_parameters_to_context(codec_ctx_, params) < 0)
            throw std::runtime_error("Decoder: Failed to copy codec params to codec context");

        if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0)
            throw std::runtime_error("Decoder: Unable to open the av codec");

    } catch (const std::exception &e) {
        cleanup();
        throw;
    }
}

Decoder::~Decoder() { cleanup(); }

void Decoder::cleanup() {
    // TO-DO: free codec_ (how?)
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

bool Decoder::sendPacket(std::shared_ptr<const AVPacket> packet) const {
    int ret = avcodec_send_packet(codec_ctx_, packet.get());
    if (ret == AVERROR(EAGAIN)) {
        return false;
    } else if (ret == AVERROR_EOF) {
        throw std::runtime_error("Decoder has already been flushed");
    } else if (ret < 0) {
        throw std::runtime_error("Failed to send packet to decoder");
    }
    return true;
}

std::shared_ptr<const AVFrame> Decoder::getFrame() const {
    auto frame = std::shared_ptr<AVFrame>(av_frame_alloc(), AVFrameDeleter());
    if (!frame) throw std::runtime_error("DEcoder: failed to allocate frame");

    int ret = avcodec_receive_frame(codec_ctx_, frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        throw std::runtime_error("Decoder: Failed to receive frame from decoder");
    }
    return frame;
}

const AVCodecContext *Decoder::getCodecContext() const { return codec_ctx_; }