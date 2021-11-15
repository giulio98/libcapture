#include "../include/demuxer.h"

Demuxer::Demuxer(const std::string &fmt_name, const std::string &device_name,
                 const std::map<std::string, std::string> &options)
    : fmt_ctx_(nullptr), device_name_(device_name), video_stream_(nullptr), audio_stream_(nullptr) {
    AVInputFormat *fmt = nullptr;
    AVDictionary *dict = nullptr;

    try {
        fmt = av_find_input_format(fmt_name.c_str());
        if (!fmt) throw std::runtime_error("Demuxer: Cannot find input format");

        for (auto const &[key, val] : options) {
            if (av_dict_set(&dict, key.c_str(), val.c_str(), 0) < 0) {
                throw std::runtime_error("Demuxer: Cannot set " + key + "in dictionary");
            }
        }

        if (avformat_open_input(&fmt_ctx_, device_name_.c_str(), fmt, dict ? &dict : nullptr))
            throw std::runtime_error("Demuxer: Cannot open input format");

        if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0)
            throw std::runtime_error("Demuxer: Failed to find stream info");

        for (int i = 0; i < fmt_ctx_->nb_streams; i++) {
            AVStream *stream = fmt_ctx_->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_ = stream;
            } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_ = stream;
            }
        }

        // TO-DO: free fmt (how?)
        if (dict) av_dict_free(&dict);

    } catch (const std::exception &e) {
        cleanup();
        // TO-DO: free fmt (how?)
        if (dict) av_dict_free(&dict);
        throw;
    }
}

Demuxer::~Demuxer() { cleanup(); }

void Demuxer::cleanup() {
    if (fmt_ctx_) avformat_close_input(&fmt_ctx_);  // This will also free the streams
}

const AVCodecParameters *Demuxer::getVideoParams() const {
    if (!video_stream_) throw std::runtime_error("Demuxer: Video stream not present");
    return video_stream_->codecpar;
}

const AVCodecParameters *Demuxer::getAudioParams() const {
    if (!audio_stream_) throw std::runtime_error("Demuxer: Audio stream not present");
    return audio_stream_->codecpar;
}

std::pair<std::shared_ptr<const AVPacket>, AVType> Demuxer::getPacket() const {
    auto packet = std::shared_ptr<AVPacket>(av_packet_alloc(), AVPacketDeleter());
    if (!packet) throw std::runtime_error("Demuxer: failed to allocate packet");

    int ret = av_read_frame(fmt_ctx_, packet.get());
    if (ret == AVERROR(EAGAIN)) return std::make_pair(nullptr, none);
    if (ret < 0) throw std::runtime_error("Demuxer: Failed to read a packet");

    AVType packet_type;
    if (video_stream_ && packet->stream_index == video_stream_->index) {
        packet_type = video;
    } else if (audio_stream_ && packet->stream_index == audio_stream_->index) {
        packet_type = audio;
    } else {
        throw std::runtime_error("Demuxer: unknown packet stream index");
    }

    return std::make_pair(packet, packet_type);
}

void Demuxer::dumpInfo() const { av_dump_format(fmt_ctx_, 0, device_name_.c_str(), 0); }