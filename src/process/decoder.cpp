#include "decoder.h"

#include <stdexcept>

static void throwRuntimeError(const std::string &msg) { throw std::runtime_error("Decoder: " + msg); }

void swap(Decoder &lhs, Decoder &rhs) {
    std::swap(lhs.codec_, rhs.codec_);
    std::swap(lhs.codec_ctx_, rhs.codec_ctx_);
    std::swap(lhs.frame_, rhs.frame_);
}

Decoder::Decoder(const AVCodecParameters *params) {
    if (!params) throwRuntimeError("received stream parameters ptr is null");

    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) throwRuntimeError("cannot find codec");

    codec_ctx_ = av::CodecContextUPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throwRuntimeError("failed to allocated memory for AVCodecContext");

    if (avcodec_parameters_to_context(codec_ctx_.get(), params) < 0)
        throwRuntimeError("failed to copy codec params to codec context");

    if (avcodec_open2(codec_ctx_.get(), codec_, nullptr) < 0) throwRuntimeError("unable to open the av codec");
}

Decoder::Decoder(Decoder &&other) noexcept { swap(*this, other); }

Decoder &Decoder::operator=(Decoder other) {
    swap(*this, other);
    return *this;
}

bool Decoder::sendPacket(const AVPacket *packet) {
    if (!codec_ctx_) throwRuntimeError("decoder was not initialized yet");
    int ret = avcodec_send_packet(codec_ctx_.get(), packet);
    if (ret == AVERROR(EAGAIN)) return false;
    if (ret == AVERROR_EOF) throwRuntimeError("has already been flushed");
    if (ret < 0) throwRuntimeError("failed to send packet to decoder");
    return true;
}

av::FrameUPtr Decoder::getFrame() {
    if (!codec_ctx_) throwRuntimeError("decoder was not initialized yet");

    if (!frame_) {
        frame_ = av::FrameUPtr(av_frame_alloc());
        if (!frame_) throwRuntimeError("failed to allocate frame");
    }

    int ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throwRuntimeError("failed to receive frame from decoder");

    return std::move(frame_);
}

const AVCodecContext *Decoder::getContext() const { return codec_ctx_.get(); }

std::string Decoder::getName() const {
    if (codec_) return codec_->long_name;
    return std::string{};
}