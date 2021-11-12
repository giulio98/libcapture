#include "../include/decoder.h"

Decoder::Decoder(const AVCodecParameters *params) : codec_ctx_(nullptr) {
    AVCodec *codec = avcodec_find_decoder(params->codec_id);
    if (!codec) throw std::runtime_error("Cannot find codec");

    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) throw std::runtime_error("Failed to allocated memory for AVCodecContext");

    if (avcodec_parameters_to_context(codec_ctx_, params) < 0)
        throw std::runtime_error("Failed to copy codec params to codec context");

    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) throw std::runtime_error("Unable to open the av codec");

    // TO-DO: free codec (with which function?)
}

Decoder::~Decoder() {
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

void Decoder::sendPacket(AVPacket *packet) {
    int ret;

    ret = avcodec_send_packet(codec_ctx_, packet);
    if (ret == AVERROR(EAGAIN)) {
        throw BufferFullException();
    } else if (ret == AVERROR_EOF) {
        throw BufferFlushedException();
    } else if (ret < 0) {
        throw std::runtime_error("Failed to send packet to decoder");
    }
}

void Decoder::fillFrame(AVFrame *frame) {
    int ret;

    if (!frame) throw std::runtime_error("Empty frame");

    ret = avcodec_receive_frame(codec_ctx_, frame);
    if (ret == AVERROR(EAGAIN)) {
        throw BufferEmptyException();
    } else if (ret == AVERROR_EOF) {
        throw BufferFlushedException();
    } else if (ret < 0) {
        throw std::runtime_error("Failed to receive frame from decoder");
    }
}