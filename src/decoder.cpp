#include "include/decoder.h"
#include <stdexcept>

Decoder::Decoder(const AVCodecParameters *params) : codec_(nullptr), codec_ctx_(nullptr) {
    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) throw std::runtime_error("Decoder: Cannot find codec");

    codec_ctx_ = av::CodecContextUPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throw std::runtime_error("Decoder: Failed to allocated memory for AVCodecContext");

    if (avcodec_parameters_to_context(codec_ctx_.get(), params) < 0)
        throw std::runtime_error("Decoder: Failed to copy codec params to codec context");

    if (avcodec_open2(codec_ctx_.get(), codec_, nullptr) < 0)
        throw std::runtime_error("Decoder: Unable to open the av codec");
}

bool Decoder::sendPacket(const AVPacket *packet) const {
    int ret = avcodec_send_packet(codec_ctx_.get(), packet);
    if (ret == AVERROR(EAGAIN)) {
        return false;
    } else if (ret == AVERROR_EOF) {
        throw std::runtime_error("Decoder has already been flushed");
    } else if (ret < 0) {
        throw std::runtime_error("Failed to send packet to decoder");
    }
    return true;
}

av::FrameUPtr Decoder::getFrame() const {
    av::FrameUPtr frame(av_frame_alloc());
    if (!frame) throw std::runtime_error("Decoder: failed to allocate frame");

    int ret = avcodec_receive_frame(codec_ctx_.get(), frame.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return nullptr;
    } else if (ret < 0) {
        throw std::runtime_error("Decoder: Failed to receive frame from decoder");
    }
    return frame;
}

const AVCodecContext *Decoder::getCodecContext() const { return codec_ctx_.get(); }