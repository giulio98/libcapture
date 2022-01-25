#include "muxer.h"

#include <stdexcept>

static std::string errMsg(const std::string &msg) { return std::string("Muxer: " + msg); }

Muxer::Muxer(std::string filename) : filename_(std::move(filename)) {
    AVFormatContext *fmt_ctx = nullptr;
    if (avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, filename_.c_str()) < 0)
        throw std::runtime_error(errMsg("failed to allocate output context for file '" + filename_ + "'"));
    fmt_ctx_ = av::FormatContextUPtr(fmt_ctx);
}

Muxer::~Muxer() {
    if (fmt_ctx_->pb) avio_closep(&(fmt_ctx_->pb));
}

void Muxer::addStream(const AVCodecContext *enc_ctx) {
    if (!enc_ctx) throw std::invalid_argument(errMsg("received encoder context is NULL"));

    av::MediaType type;
    if (enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        type = av::MediaType::Video;
    } else if (enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        type = av::MediaType::Audio;
    } else {
        throw std::invalid_argument(errMsg("received encoder context is of unknown media type"));
    }

    if (file_opened_) throw std::logic_error(errMsg("cannot add a new stream, file has already been opened"));
    if (streams_[type]) throw std::logic_error(errMsg("stream of specified type already present"));

    const AVStream *stream = avformat_new_stream(fmt_ctx_.get(), nullptr);
    if (!stream) throw std::runtime_error(errMsg("failed to create a new stream"));

    if (avcodec_parameters_from_context(stream->codecpar, enc_ctx) < 0)
        throw std::runtime_error(errMsg("failed to write video stream parameters"));

    streams_[type] = stream;
    encoders_time_bases_[type] = enc_ctx->time_base;
}

void Muxer::openFile() {
    if (file_opened_) throw std::logic_error(errMsg("cannot open file, file has already been opened"));
    /* create empty video file */
    if (!(fmt_ctx_->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, filename_.c_str(), AVIO_FLAG_WRITE) < 0) {
            throw std::runtime_error(errMsg("failed to create the output file"));
        }
    }
    file_opened_ = true;
    if (avformat_write_header(fmt_ctx_.get(), nullptr) < 0)
        throw std::runtime_error(errMsg("Failed to write file header"));
}

void Muxer::closeFile() {
    if (!file_opened_) throw std::logic_error(errMsg("cannot close file, file has not been opened"));
    if (file_closed_) throw std::logic_error(errMsg("cannot close file, file has already been closed"));
    if (av_write_trailer(fmt_ctx_.get()) < 0) throw std::runtime_error(errMsg("failed to write file trailer"));
    if (fmt_ctx_->pb) {
        if (avio_closep(&(fmt_ctx_->pb)) < 0) throw std::runtime_error(errMsg("failed to close file"));
    }
    file_closed_ = true;
}

bool Muxer::isInited() const { return file_opened_; }

void Muxer::writePacket(const av::PacketUPtr packet, const av::MediaType packet_type) {
    if (!file_opened_) throw std::logic_error(errMsg("cannot write packet, file has not been opened"));
    if (file_closed_) throw std::logic_error(errMsg("cannot write packet, file has already been closed"));

    if (packet) {
        if (!av::validMediaType(packet_type)) throw std::invalid_argument(errMsg("received packet of unknown type"));
        auto stream = streams_[packet_type];
        if (!stream) throw std::logic_error(errMsg("stream of specified type not present"));
        av_packet_rescale_ts(packet.get(), encoders_time_bases_[packet_type], stream->time_base);
        packet->stream_index = stream->index;
    }

    if (av_interleaved_write_frame(fmt_ctx_.get(), packet.get()))
        throw std::runtime_error(errMsg("failed to write packet"));
}

void Muxer::printInfo() const { av_dump_format(fmt_ctx_.get(), 0, filename_.c_str(), 1); }

int Muxer::getGlobalHeaderFlags() const { return fmt_ctx_->oformat->flags; }