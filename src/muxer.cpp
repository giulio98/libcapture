#include "../include/muxer.h"

#include <iostream>

Muxer::Muxer(const std::string &filename)
    : fmt_ctx_(nullptr), filename_(filename), video_stream_(nullptr), audio_stream_(nullptr) {
    avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, filename_.c_str());
}

Muxer::~Muxer() {
    if (fmt_ctx_) avformat_free_context(fmt_ctx_);
}

AVStream *Muxer::addVideoStream() {
    if (!video_stream_) {
        video_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        if (!video_stream_) {
            std::cerr << "Error in creating a av format new stream" << std::endl;
        }
    }
    return video_stream_;
}

AVStream *Muxer::addAudioStream() {
    if (!audio_stream_) {
        audio_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        if (!audio_stream_) {
            std::cerr << "Error in creating a av format new stream" << std::endl;
        }
    }
    return audio_stream_;
}

void Muxer::writeHeader() {
    /* create empty video file */
    if (!(fmt_ctx_->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, filename_.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Error in creating the output file" << std::endl;
        }
    }

    if (avformat_write_header(fmt_ctx_, NULL)) {
        std::cerr << "Error in writing the header context" << std::endl;
    }
}

void Muxer::writePacket(AVPacket *packet) {
    if (packet && av_interleaved_write_frame(fmt_ctx_, packet)) {
        std::cerr << "Error in writing video frame" << std::endl;
    }
}

void Muxer::writeTrailer() {
    if (av_write_trailer(fmt_ctx_)) {
        std::cerr << "Error in writing av trailer" << std::endl;
    } else {
        if (avio_close(fmt_ctx_->pb)) std::cerr << "Error in closing file" << std::endl;
    }
}

void Muxer::dumpInfo() { av_dump_format(fmt_ctx_, 0, filename_.c_str(), 1); }