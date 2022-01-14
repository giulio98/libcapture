#include "muxer.h"

#include <iostream>
#include <stdexcept>

static void throw_error(const std::string &msg) { throw std::runtime_error("Muxer: " + msg); }

Muxer::Muxer(std::string filename) : filename_(std::move(filename)) {
    AVFormatContext *fmt_ctx = nullptr;
    if (avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, filename_.c_str()) < 0)
        throw_error("failed to allocate output format context");
    fmt_ctx_ = av::FormatContextUPtr(fmt_ctx);
}

Muxer::~Muxer() {
    if (fmt_ctx_ && fmt_ctx_->pb) avio_closep(&(fmt_ctx_->pb));
}

void Muxer::addStream(const AVCodecContext *enc_ctx) {
    if (!enc_ctx) throw_error("received encoder context is NULL");

    av::MediaType type;
    if (enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        type = av::MediaType::Video;
    } else if (enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        type = av::MediaType::Audio;
    } else {
        throw_error("received encoder context is ok unknown media type");
    }

    std::unique_lock ul{m_};

    if (file_opened_) throw_error("cannot add a new stream, file has already been opened");

    auto stream = streams_[type];

    if (stream) throw_error("stream of specified type already present");
    stream = avformat_new_stream(fmt_ctx_.get(), nullptr);
    if (!stream) throw_error("failed to create a new stream");

    if (avcodec_parameters_from_context(stream->codecpar, enc_ctx) < 0)
        throw_error("failed to write video stream parameters");

    streams_[type] = stream;
    encoders_time_bases_[type] = enc_ctx->time_base;
}

void Muxer::openFile() {
    std::unique_lock ul{m_};
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
    std::unique_lock ul{m_};
    if (!file_opened_) throw_error("cannot close file, file has not been opened");
    if (file_closed_) throw_error("cannot close file, file has already been closed");
    if (av_interleaved_write_frame(fmt_ctx_.get(), nullptr)) throw_error("failed to flush internal packet queue");
    if (av_write_trailer(fmt_ctx_.get()) < 0) throw_error("failed to write file trailer");
    if (fmt_ctx_->pb) {
        if (avio_closep(&(fmt_ctx_->pb)) < 0) throw_error("failed to close file");
    }
    file_closed_ = true;
}

void Muxer::writePacket(av::PacketUPtr packet, av::MediaType packet_type) {
    std::unique_lock ul{m_};

    if (!file_opened_) throw_error("cannot write packet, file has not been opened");
    if (file_closed_) throw_error("cannot write packet, file has already been closed");

    if (packet) {
        if (!av::validMediaType(packet_type)) throw_error("received packet of unknown type");
        auto stream = streams_[packet_type];
        if (!stream) throw_error("stream of specified type not present");
        av_packet_rescale_ts(packet.get(), encoders_time_bases_[packet_type], stream->time_base);
        packet->stream_index = stream->index;
    }

    if (av_interleaved_write_frame(fmt_ctx_.get(), packet.get())) throw_error("failed to write packet");
}

void Muxer::printInfo() {
    std::unique_lock ul{m_};
    av_dump_format(fmt_ctx_.get(), 0, filename_.c_str(), 1);
}

int Muxer::getGlobalHeaderFlags() {
    std::unique_lock ul{m_};
    return fmt_ctx_->oformat->flags;
}