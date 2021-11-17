#include "../include/demuxer.h"

Demuxer::Demuxer(const std::string &fmt_name, const std::string &device_name,
                 const std::map<std::string, std::string> &options)
    : fmt_ctx_(nullptr), device_name_(device_name), video_stream_(nullptr), audio_stream_(nullptr) {
    auto fmt = av_find_input_format(fmt_name.c_str());
    if (!fmt) throw std::runtime_error("Demuxer: Cannot find input format");

    {
        AVFormatContext *fmt_ctx = nullptr;
        auto dict = av::map2dict(options).release();
        int err = avformat_open_input(&fmt_ctx, device_name_.c_str(), fmt, dict ? &dict : nullptr);
        av_dict_free(&dict);
        fmt_ctx_ = av::InFormatContextPtr(fmt_ctx);
        if (err) throw std::runtime_error("Demuxer: Cannot open input format");
    }

    if (avformat_find_stream_info(fmt_ctx_.get(), nullptr) < 0)
        throw std::runtime_error("Demuxer: Failed to find stream info");

    for (int i = 0; i < fmt_ctx_->nb_streams; i++) {
        const AVStream *stream = fmt_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_ = stream;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_ = stream;
        }
    }
}

const AVCodecParameters *Demuxer::getVideoParams() const {
    if (!video_stream_) throw std::runtime_error("Demuxer: Video stream not present");
    return video_stream_->codecpar;
}

const AVCodecParameters *Demuxer::getAudioParams() const {
    if (!audio_stream_) throw std::runtime_error("Demuxer: Audio stream not present");
    return audio_stream_->codecpar;
}

std::pair<std::shared_ptr<const AVPacket>, av::DataType> Demuxer::readPacket() const {
    std::shared_ptr<AVPacket> packet(av_packet_alloc(), DeleterPP<av_packet_free>());
    if (!packet) throw std::runtime_error("Demuxer: failed to allocate packet");

    int ret = av_read_frame(fmt_ctx_.get(), packet.get());
    if (ret == AVERROR(EAGAIN)) return std::make_pair(nullptr, av::DataType::none);
    if (ret < 0) throw std::runtime_error("Demuxer: Failed to read a packet");

    av::DataType packet_type;
    if (video_stream_ && packet->stream_index == video_stream_->index) {
        packet_type = av::DataType::video;
    } else if (audio_stream_ && packet->stream_index == audio_stream_->index) {
        packet_type = av::DataType::audio;
    } else {
        throw std::runtime_error("Demuxer: unknown packet stream index");
    }

    return std::make_pair(packet, packet_type);
}

void Demuxer::flush() const {
    if (avformat_flush(fmt_ctx_.get()) < 0) throw std::runtime_error("Demuxer: failed to flush");
}

void Demuxer::dumpInfo() const { av_dump_format(fmt_ctx_.get(), 0, device_name_.c_str(), 0); }