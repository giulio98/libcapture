#include "decoder.h"

#include <stdexcept>

static std::string errMsg(const std::string &msg) { return ("Decoder: " + msg); }

void swap(Decoder &lhs, Decoder &rhs) {
    std::swap(lhs.codec_, rhs.codec_);
    std::swap(lhs.codec_ctx_, rhs.codec_ctx_);
    std::swap(lhs.frame_, rhs.frame_);
}

Decoder::Decoder(const AVCodecParameters *params) {
    if (!params) throw std::invalid_argument(errMsg("received stream parameters ptr is null"));

    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) throw std::runtime_error(errMsg("cannot find decoder"));

    codec_ctx_ = av::CodecContextUPtr(avcodec_alloc_context3(codec_));
    if (!codec_ctx_) throw std::runtime_error(errMsg("failed to allocated memory for AVCodecContext"));

    if (avcodec_parameters_to_context(codec_ctx_.get(), params) < 0)
        throw std::runtime_error(errMsg("failed to copy codec params to codec context"));

    if (avcodec_open2(codec_ctx_.get(), codec_, nullptr) < 0)
        throw std::runtime_error(errMsg("unable to open the av codec"));
}

Decoder::Decoder(Decoder &&other) noexcept { swap(*this, other); }

Decoder &Decoder::operator=(Decoder other) {
    swap(*this, other);
    return *this;
}

bool Decoder::sendPacket(const AVPacket *packet) {
    if (!codec_ctx_) throw std::logic_error(errMsg("decoder was not initialized yet"));
    const int ret = avcodec_send_packet(codec_ctx_.get(), packet);
    if (ret == AVERROR(EAGAIN)) return false;
    if (ret == AVERROR_EOF) throw std::logic_error(errMsg("has already been flushed"));
    if (ret < 0) throw std::runtime_error(errMsg("failed to send packet to decoder"));
    return true;
}

av::FrameUPtr Decoder::getFrame() {
    if (!codec_ctx_) throw std::logic_error(errMsg("decoder was not initialized yet"));

    if (!frame_) {
        frame_ = av::FrameUPtr(av_frame_alloc());
        if (!frame_) throw std::runtime_error(errMsg("failed to allocate frame"));
    }

    const int ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return nullptr;
    if (ret < 0) throw std::runtime_error(errMsg("failed to receive frame from decoder"));

    return std::move(frame_);
}

const AVCodecContext *Decoder::getContext() const { return codec_ctx_.get(); }

std::string Decoder::getName() const {
    if (codec_) return codec_->long_name;
    return std::string{};
}