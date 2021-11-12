#include "../include/encoder.h"

Encoder::Encoder(AVCodecID codec_id, std::map<std::string, std::string> options, int global_header_flags)
    : codec_ctx_(nullptr) {
    int ret;

    AVCodec *codec = avcodec_find_decoder(codec_id);
    if (!codec) throw std::runtime_error("Cannot find codec");

    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) throw std::runtime_error("Failed to allocated memory for AVCodecContext");

    AVDictionary **dict_ptr = nullptr;

    for (auto const &[key, val] : options) {
        if (av_dict_set(dict_ptr, key.c_str(), val.c_str(), 0) < 0) {
            if (*dict_ptr) av_dict_free(dict_ptr);
            throw std::runtime_error("Cannot set " + key + "in dictionary");
        }
    }

    ret = avcodec_open2(codec_ctx_, codec, dict_ptr);
    if (*dict_ptr) av_dict_free(dict_ptr);
    // TO-DO: free codec (how?)
    if (ret < 0) throw std::runtime_error("Failed to initialize Codec Context");

    if (global_header_flags & AVFMT_GLOBALHEADER) codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

Encoder::~Encoder() {
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

void Encoder::sendFrame(AVFrame *frame) {
    int ret;

    ret = avcodec_send_frame(codec_ctx_, frame);
    if (ret == AVERROR(EAGAIN)) {
        throw BufferFullException();
    } else if (ret == AVERROR_EOF) {
        throw BufferFlushedException();
    } else if (ret < 0) {
        throw std::runtime_error("Failed to send frame to encoder");
    }
}

void Encoder::fillPacket(AVPacket *packet) {
    int ret;

    if (!packet) throw std::runtime_error("Frame is not allocated");

    ret = avcodec_receive_packet(codec_ctx_, packet);
    if (ret == AVERROR(EAGAIN)) {
        throw BufferEmptyException();
    } else if (ret == AVERROR_EOF) {
        throw BufferFlushedException();
    } else if (ret < 0) {
        throw std::runtime_error("Failed to receive frame from decoder");
    }
}

const AVCodecContext *Encoder::getCodecContext() { return codec_ctx_; }