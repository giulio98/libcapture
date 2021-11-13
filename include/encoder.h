#pragma once

#include <map>
#include <string>

#include "ffmpeg_libs.h"

class Encoder {
protected:
    AVCodec *codec_;
    AVCodecContext *codec_ctx_;
    AVDictionary *options_;

    Encoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int global_header_flags);

public:
    ~Encoder();

    /**
     * Send an allocated frame to the encoder
     * The owneship of the frame remains to the caller
     * @return true if the frame has been correctly sent, false if the encoder could not receive it
     */
    bool sendFrame(const AVFrame *frame);

    /**
     * Fill an allocated packet obtained by encoding frames
     * The owneship of the packet remains to the caller
     * @return true if the packet has been correctly filled, false if the encoder had nothing to write
     */
    bool fillPacket(AVPacket *packet);

    const AVCodecContext *getCodecContext();
};