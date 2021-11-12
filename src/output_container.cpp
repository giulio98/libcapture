#include "../include/output_container.h"

#include <iostream>

OutputContainer::OutputContainer(const std::string &filename)
    : fmt_ctx_(nullptr), filename_(filename), video_stream_(nullptr), audio_stream_(nullptr) {
    avformat_alloc_output_context2(&fmt_ctx_, nullptr, nullptr, filename_.c_str());
}

OutputContainer::~OutputContainer() {}

AVStream *OutputContainer::addVideoStream() {
    if (!video_stream_) {
        video_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        if (!video_stream_) {
            std::cerr << "Error in creating a av format new stream" << std::endl;
        }
    }
    return video_stream_;
}

AVStream *OutputContainer::addAudioStream() {
    if (!audio_stream_) {
        audio_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        if (!audio_stream_) {
            std::cerr << "Error in creating a av format new stream" << std::endl;
        }
    }
    return audio_stream_;
}

void OutputContainer::writeHeader() {
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

void OutputContainer::writePacket(AVPacket *packet) {
    if (packet && av_interleaved_write_frame(fmt_ctx_, packet)) {
        std::cerr << "Error in writing video frame" << std::endl;
    }
}

void OutputContainer::writeTrailer() {
    if (av_write_trailer(fmt_ctx_)) {
        std::cerr << "Error in writing av trailer" << std::endl;
    } else {
        if (avio_close(fmt_ctx_->pb)) std::cerr << "Error in closing file" << std::endl;
    }
}

void OutputContainer::dumpInfo() { av_dump_format(fmt_ctx_, 0, filename_.c_str(), 1); }