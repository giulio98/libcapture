#include "demuxer.h"

#include <iostream>
#include <stdexcept>

#define VERBOSE 0

static void throw_error(const std::string &msg) { throw std::runtime_error("Demuxer: " + msg); }

void swap(Demuxer &lhs, Demuxer &rhs) {
    std::swap(lhs.fmt_ctx_, rhs.fmt_ctx_);
    std::swap(lhs.fmt_, rhs.fmt_);
    std::swap(lhs.device_name_, rhs.device_name_);
    std::swap(lhs.options_, rhs.options_);
    std::swap(lhs.streams_[av::MediaType::Audio], rhs.streams_[av::MediaType::Audio]);
    std::swap(lhs.streams_[av::MediaType::Video], rhs.streams_[av::MediaType::Video]);
    std::swap(lhs.packet_, rhs.packet_);
}

Demuxer::Demuxer(const std::string &fmt_name, std::string device_name, std::map<std::string, std::string> options)
    : device_name_(std::move(device_name)), options_(std::move(options)) {
    fmt_ = av_find_input_format(fmt_name.c_str());
    if (!fmt_) throw_error("cannot find input format");
}

Demuxer::Demuxer(Demuxer &&other) : Demuxer() { swap(*this, other); }

Demuxer &Demuxer::operator=(Demuxer other) {
    swap(*this, other);
    return *this;
}

void Demuxer::openInput() {
    if (fmt_ctx_) throw_error("failed to open input device (input is already open)");
    if (!fmt_) throw_error("failed to open input device (missing input format)");

    {
        AVFormatContext *fmt_ctx = nullptr;
        av::DictionaryUPtr dict = av::map2dict(options_);
        AVDictionary *dict_raw = dict.release();
        int ret = avformat_open_input(&fmt_ctx, device_name_.c_str(), fmt_, dict_raw ? &dict_raw : nullptr);
        dict = av::DictionaryUPtr(dict_raw);
        fmt_ctx_ = av::InFormatContextUPtr(fmt_ctx);
        if (ret) throw_error("cannot open input format");
#if VERBOSE
        auto map = av::dict2map(dict.get());
        for (const auto &[key, val] : map) {
            std::cerr << "Encoder: couldn't find any '" << key << "' option" << std::endl;
        }
#endif
    }

    if (avformat_find_stream_info(fmt_ctx_.get(), nullptr) < 0) throw_error("Failed to find stream info");

    for (int i = 0; i < fmt_ctx_->nb_streams; i++) {
        const AVStream *stream = fmt_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            streams_[av::MediaType::Video] = stream;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            streams_[av::MediaType::Audio] = stream;
        }
    }
}

void Demuxer::closeInput() {
    if (!fmt_ctx_) throw_error("failed to close input (input is not open)");
    fmt_ctx_.reset();
    streams_[av::MediaType::Video] = nullptr;
    streams_[av::MediaType::Audio] = nullptr;
}

void Demuxer::flush() {
    if (!fmt_ctx_) throw_error("failed to flush (input is not open)");
    if (fmt_ctx_->pb) avio_flush(fmt_ctx_->pb);
    if (avformat_flush(fmt_ctx_.get()) < 0) throw_error("failed to flush internal data");
}

bool Demuxer::isInputOpen() const { return (fmt_ctx_ != nullptr); }

const AVCodecParameters *Demuxer::getStreamParams(av::MediaType stream_type) const {
    if (!fmt_ctx_) throw_error("failed to acess stream (input is not open)");
    if (!av::validMediaType(stream_type)) throw_error("invalid stream_type received");
    if (!streams_[stream_type]) throw_error("specified stream not present");
    return streams_[stream_type]->codecpar;
}

AVRational Demuxer::getStreamTimeBase(av::MediaType stream_type) const {
    if (!fmt_ctx_) throw_error("failed to acess stream (input is not open)");
    if (!av::validMediaType(stream_type)) throw_error("invalid stream_type received");
    if (!streams_[stream_type]) throw_error("specified stream not present");
    return streams_[stream_type]->time_base;
}

std::pair<av::PacketUPtr, av::MediaType> Demuxer::readPacket() {
    if (!fmt_ctx_) throw_error("failed to read packet (input is not open)");

    if (!packet_) {
        packet_ = av::PacketUPtr(av_packet_alloc());
        if (!packet_) throw_error("failed to allocate packet");
    }

    auto packet_type = av::MediaType::None;

    int ret = av_read_frame(fmt_ctx_.get(), packet_.get());
    if (ret == AVERROR(EAGAIN)) return std::make_pair(nullptr, packet_type);
    if (ret < 0) throw_error("failed to read a packet");

    for (auto type : av::validMediaTypes) {
        if (streams_[type] && packet_->stream_index == streams_[type]->index) {
            packet_type = type;
            break;
        }
    }
    if (packet_type == av::MediaType::None) throw_error("unknown packet stream index");

    return std::make_pair(std::move(packet_), packet_type);
}

void Demuxer::printInfo(int index) const {
    if (!fmt_ctx_) throw_error("failed to print info (input is not open)");
    av_dump_format(fmt_ctx_.get(), index, device_name_.c_str(), 0);
}