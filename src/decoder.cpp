#include "../include/decoder.h"

#include <iostream>

// AVCodec *codec_;
// AVCodecContext *codec_ctx_;

Decoder::Decoder(AVCodecID codec_id) : codec_(nullptr), codec_ctx_(nullptr) {
    codec_ = avcodec_find_decoder(codec_id);
    if (!codec_) {
        std::cerr << "Unable to find the video decoder" << std::endl;
        return;
    }

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        std::cerr << "Failed to allocated memory for AVCodecContext" << std::endl;
        return;
    }
}

Decoder::~Decoder() {
    // TO-DO: free codec_ (with which function?)
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

void Decoder::setParamsFromStream(AVStream *stream) {
    if (avcodec_parameters_to_context(codec_ctx_, stream->codecpar) < 0) {
        std::cerr << "Failed to copy codec params to codec context" << std::endl;
        return;
    }
}

int Decoder::sendPacket(AVPacket *packet) {
    if (!packet) {
        std::cerr << "ERROR: packet not allocated" << std::endl;
        return -1;
    }
    return avcodec_send_packet(codec_ctx_, packet);
}

// TO-DO: adjust using exceptions instead of returning an integer
int Decoder::fillFrame(AVFrame *frame) {
    if (!frame) {
        std::cerr << "ERROR: frame not allocated" << std::endl;
        return -1;
    }
    return avcodec_receive_frame(codec_ctx_, frame);
}