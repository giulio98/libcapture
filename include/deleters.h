#pragma once

#include <iostream>

#include "ffmpeg_libs.h"

struct AVPacketDeleter {
    void operator()(AVPacket *packet) { av_packet_free(&packet); }
};

struct AVFrameDeleter {
    void operator()(AVFrame *frame) { av_frame_free(&frame); }
};