#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

struct AVPacketDeleter {
    void operator()(AVPacket *packet) {
        if (packet) av_packet_free(&packet);
        std::cout << "AVPacketDeleter released a packet!" << std::endl;
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame *frame) {
        if (frame) av_frame_free(&frame);
        std::cout << "AVFrameDeleter released a frame!" << std::endl;
    }
};