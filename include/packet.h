#pragma once

#include <libavcodec/avcodec.h>

class Packet {
    AVPacket* packet_;

public:
    Packet() { packet_ = av_packet_alloc(); }

    ~Packet() { av_packet_free(&packet_); }

    void Unref() { av_packet_unref(packet_); }

    AVPacket* operator->() { return packet_; }

    AVPacket* Get() { return packet_; }
};