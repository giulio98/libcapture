#include "demuxer.h"

#include <iostream>
#include <stdexcept>

#define VERBOSE 0  // TO-DO: improve

static void throwLogicError(const std::string &msg) { throw std::logic_error("Demuxer: " + msg); }

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
    : fmt_(av_find_input_format(fmt_name.c_str())), device_name_(std::move(device_name)), options_(std::move(options)) {
    if (!fmt_) throwLogicError("cannot find input format");
}

Demuxer::Demuxer(Demuxer &&other) noexcept : Demuxer() { swap(*this, other); }

Demuxer &Demuxer::operator=(Demuxer other) {
    swap(*this, other);
    return *this;
}

void Demuxer::openInput() {
    if (fmt_ctx_) throwLogicError("failed to open input device (input is already open)");
    if (!fmt_) throwLogicError("failed to open input device (missing input format)");

    {
        AVFormatContext *fmt_ctx = nullptr;
        av::DictionaryUPtr dict = av::map2dict(options_);
        AVDictionary *dict_raw = dict.release();
        int ret = avformat_open_input(&fmt_ctx, device_name_.c_str(), fmt_, dict_raw ? &dict_raw : nullptr);
        dict = av::DictionaryUPtr(dict_raw);
        fmt_ctx_ = av::InFormatContextUPtr(fmt_ctx);
        if (ret) throwLogicError("cannot open input format");
#if VERBOSE
        auto map = av::dict2map(dict.get());
        for (const auto &[key, val] : map) {
            std::cerr << "Encoder: couldn't find any '" << key << "' option" << std::endl;
        }
#endif
    }

    if (avformat_find_stream_info(fmt_ctx_.get(), nullptr) < 0) throwLogicError("Failed to find stream info");

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
    if (!fmt_ctx_) throwLogicError("failed to close input (input is not open)");
    fmt_ctx_.reset();
    streams_[av::MediaType::Video] = nullptr;
    streams_[av::MediaType::Audio] = nullptr;
}

void Demuxer::flush() {
    if (!fmt_ctx_) throwLogicError("failed to flush (input is not open)");
    if (fmt_ctx_->pb) avio_flush(fmt_ctx_->pb);
    if (avformat_flush(fmt_ctx_.get()) < 0) throwLogicError("failed to flush internal data");
}

bool Demuxer::isInputOpen() const { return (fmt_ctx_ != nullptr); }

const AVCodecParameters *Demuxer::getStreamParams(const av::MediaType stream_type) const {
    if (!fmt_ctx_) throwLogicError("failed to acess stream (input is not open)");
    if (!av::validMediaType(stream_type)) throwLogicError("invalid stream_type received");
    if (!streams_[stream_type]) throwLogicError("specified stream not present");
    return streams_[stream_type]->codecpar;
}

AVRational Demuxer::getStreamTimeBase(const av::MediaType stream_type) const {
    if (!fmt_ctx_) throwLogicError("failed to acess stream (input is not open)");
    if (!av::validMediaType(stream_type)) throwLogicError("invalid stream_type received");
    if (!streams_[stream_type]) throwLogicError("specified stream not present");
    return streams_[stream_type]->time_base;
}

std::pair<av::PacketUPtr, av::MediaType> Demuxer::readPacket() {
    if (!fmt_ctx_) throwLogicError("failed to read packet (input is not open)");

    if (!packet_) {
        packet_ = av::PacketUPtr(av_packet_alloc());
        if (!packet_) throwLogicError("failed to allocate packet");
    }

    auto packet_type = av::MediaType::None;

    int ret = av_read_frame(fmt_ctx_.get(), packet_.get());
    if (ret == AVERROR(EAGAIN)) return std::make_pair(nullptr, packet_type);
    if (ret < 0) throwLogicError("failed to read a packet");

    for (auto type : av::validMediaTypes) {
        if (streams_[type] && packet_->stream_index == streams_[type]->index) {
            packet_type = type;
            break;
        }
    }
    if (packet_type == av::MediaType::None) throwLogicError("unknown packet stream index");

    return std::make_pair(std::move(packet_), packet_type);
}

void Demuxer::printInfo(const int index) const {
    if (!fmt_ctx_) throwLogicError("failed to print info (input is not open)");
    av_dump_format(fmt_ctx_.get(), index, device_name_.c_str(), 0);
}