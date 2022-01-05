#include "muxer.h"

#include <iostream>
#include <stdexcept>

static void throw_error(const std::string &msg) { throw std::runtime_error("Muxer: " + msg); }

Muxer::Muxer(std::string filename)
    : filename_(std::move(filename)),
      video_stream_(nullptr),
      audio_stream_(nullptr),
      file_opened_(false),
      file_closed_(false) {
    AVFormatContext *fmt_ctx = nullptr;
    if (avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, filename_.c_str()) < 0)
        throw_error("failed to allocate output format context");
    fmt_ctx_ = av::FormatContextUPtr(fmt_ctx);
}

Muxer::~Muxer() {
    if (file_opened_ && !file_closed_) {
        std::cerr << "Demuxer: WARNING, the output file " << filename_
                  << " has not been closed, trying to close now...";
        if (avio_close(fmt_ctx_->pb) < 0) std::cerr << " failed to close file";
        std::cerr << std::endl;
    }
}

void Muxer::addVideoStream(const AVCodecContext *enc_ctx) {
    if (file_opened_) throw_error("cannot add a new stream, file has already been opened");
    if (video_stream_) throw_error("video stream already added");

    video_stream_ = avformat_new_stream(fmt_ctx_.get(), nullptr);
    if (!video_stream_) throw_error("failed to create a new video stream");

    if (avcodec_parameters_from_context(video_stream_->codecpar, enc_ctx) < 0)
        throw_error("failed to write video stream parameters");

    video_enc_time_base_ = enc_ctx->time_base;
}

void Muxer::addAudioStream(const AVCodecContext *enc_ctx) {
    if (file_opened_) throw_error("cannot add a new stream, file has already been opened");
    if (audio_stream_) throw_error("audio stream already added");

    audio_stream_ = avformat_new_stream(fmt_ctx_.get(), nullptr);
    if (!audio_stream_) throw_error("failed to create a new audio stream");

    if (avcodec_parameters_from_context(audio_stream_->codecpar, enc_ctx) < 0)
        throw_error("failed to write audio stream parameters");

    audio_enc_time_base_ = enc_ctx->time_base;
}

void Muxer::openFile() {
    if (file_opened_) throw_error("cannot open file, file has already been opened");
    if (file_closed_) throw_error("cannot re-open file, file has already been closed");
    /* create empty video file */
    if (!(fmt_ctx_->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, filename_.c_str(), AVIO_FLAG_WRITE) < 0) {
            throw_error("failed to create the output file");
        }
    }
    file_opened_ = true;
    if (avformat_write_header(fmt_ctx_.get(), nullptr) < 0) throw_error("Failed to write file header");
}

void Muxer::closeFile() {
    if (!file_opened_) throw_error("cannot close file, file has not been opened");
    if (file_closed_) throw_error("cannot close file, file has already been closed");
    if (av_write_trailer(fmt_ctx_.get()) < 0) throw_error("failed to write file trailer");
    if (avio_close(fmt_ctx_->pb) < 0) throw_error("failed to close file");
    file_closed_ = true;
}

void Muxer::writePacket(av::PacketUPtr packet, av::DataType packet_type) {
    if (!file_opened_) throw_error("cannot write packet, file has not been opened");
    if (file_closed_) throw_error("cannot write packet, file has already been closed");

    if (packet) {
        if (packet_type == av::DataType::Video) {
            if (!video_stream_) throw_error("video stream not present");
            av_packet_rescale_ts(packet.get(), video_enc_time_base_, video_stream_->time_base);
            packet->stream_index = video_stream_->index;
        } else if (packet_type == av::DataType::Audio) {
            if (!audio_stream_) throw_error("audio stream not present");
            av_packet_rescale_ts(packet.get(), audio_enc_time_base_, audio_stream_->time_base);
            packet->stream_index = audio_stream_->index;
        } else {
            throw_error("received packet is of unknown type");
        }
    }

    std::unique_lock ul{m_};
    if (av_interleaved_write_frame(fmt_ctx_.get(), packet.get())) throw_error("failed to write packet");
}

void Muxer::dumpInfo() const { av_dump_format(fmt_ctx_.get(), 0, filename_.c_str(), 1); }

int Muxer::getGlobalHeaderFlags() const { return fmt_ctx_->oformat->flags; }