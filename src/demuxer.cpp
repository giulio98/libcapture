#include "demuxer.h"

#include <stdexcept>

static void throw_error(const std::string &msg) { throw std::runtime_error("Demuxer: " + msg); }

Demuxer::Demuxer(const std::string &fmt_name, std::string device_name, std::map<std::string, std::string> options)
    : fmt_(nullptr),
      device_name_(std::move(device_name)),
      options_(std::move(options)),
      video_stream_(nullptr),
      audio_stream_(nullptr) {
    fmt_ = av_find_input_format(fmt_name.c_str());
    if (!fmt_) throw_error("cannot find input format");
}

void Demuxer::openInput() {
    if (fmt_ctx_) throw_error("input is already open");

    {
        AVFormatContext *fmt_ctx = nullptr;
        av::DictionaryUPtr dict = av::map2dict(options_);
        AVDictionary *dict_raw = dict.release();
        int ret = avformat_open_input(&fmt_ctx, device_name_.c_str(), fmt_, dict_raw ? &dict_raw : nullptr);
        dict = av::DictionaryUPtr(dict_raw);
        fmt_ctx_ = av::InFormatContextUPtr(fmt_ctx);
        if (ret) throw_error("cannot open input format");
    }

    if (avformat_find_stream_info(fmt_ctx_.get(), nullptr) < 0) throw_error("Failed to find stream info");

    for (int i = 0; i < fmt_ctx_->nb_streams; i++) {
        const AVStream *stream = fmt_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_ = stream;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_ = stream;
        }
    }
}

void Demuxer::closeInput() {
    if (!fmt_ctx_) throw_error("input is not open");
    fmt_ctx_.reset();
    video_stream_ = nullptr;
    audio_stream_ = nullptr;
}

const AVCodecParameters *Demuxer::getVideoParams() const {
    if (!fmt_ctx_) throw_error("input is not open");
    if (!video_stream_) throw_error("video stream not present");
    return video_stream_->codecpar;
}

const AVCodecParameters *Demuxer::getAudioParams() const {
    if (!fmt_ctx_) throw_error("input is not open");
    if (!audio_stream_) throw_error("audio stream not present");
    return audio_stream_->codecpar;
}

std::pair<av::PacketUPtr, av::DataType> Demuxer::readPacket() const {
    if (!fmt_ctx_) throw_error("input is not open");

    av::PacketUPtr packet(av_packet_alloc());
    if (!packet) throw_error("failed to allocate packet");

    av::DataType packet_type;

    int ret = av_read_frame(fmt_ctx_.get(), packet.get());
    if (ret == AVERROR(EAGAIN)) return std::make_pair(nullptr, packet_type);
    if (ret < 0) throw_error("failed to read a packet");

    if (video_stream_ && packet->stream_index == video_stream_->index) {
        packet_type = av::DataType::Video;
    } else if (audio_stream_ && packet->stream_index == audio_stream_->index) {
        packet_type = av::DataType::Audio;
    } else {
        throw_error("unknown packet stream index");
    }

    return std::make_pair(std::move(packet), packet_type);
}

void Demuxer::dumpInfo(int index) const {
    if (!fmt_ctx_) throw_error("input is not open");
    av_dump_format(fmt_ctx_.get(), index, device_name_.c_str(), 0);
}