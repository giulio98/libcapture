#include "../include/demuxer.h"

Demuxer::Demuxer(const std::string &fmt_name, const std::string &device_name,
                 const std::map<std::string, std::string> &options)
    : fmt_ctx_(nullptr), device_name_(device_name), video_stream_(nullptr), audio_stream_(nullptr) {
    AVInputFormat *fmt = av_find_input_format(fmt_name.c_str());
    if (!fmt) throw std::runtime_error("Demuxer: Cannot find input format");

    AVDictionary *dict = nullptr;
    for (auto const &[key, val] : options) {
        if (av_dict_set(&dict, key.c_str(), val.c_str(), 0) < 0) {
            /* TO-DO: free fmt (how?) */
            if (dict) av_dict_free(&dict);
            throw std::runtime_error("Demuxer: Cannot set " + key + "in dictionary");
        }
    }

    int ret = avformat_open_input(&fmt_ctx_, device_name_.c_str(), fmt, dict ? &dict : nullptr);
    /* TO-DO: free fmt (how?) */
    if (dict) av_dict_free(&dict);
    if (ret) throw std::runtime_error("Demuxer: Cannot open input format");

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
}

Demuxer::~Demuxer() {
    if (fmt_ctx_) avformat_close_input(&fmt_ctx_);  // This will also free the streams
}

const AVStream *Demuxer::getVideoStream() {
    if (!video_stream_) throw std::runtime_error("Demuxer: Video stream not present");
    return video_stream_;
}

const AVStream *Demuxer::getAudioStream() {
    if (!audio_stream_) throw std::runtime_error("Demuxer: Audio stream not present");
    return audio_stream_;
}

void Demuxer::fillPacket(AVPacket *packet) {
    if (!packet) throw std::runtime_error("Demuxer: Packet not allocated");

    int ret = av_read_frame(fmt_ctx_, packet);
    if (ret == AVERROR(EAGAIN)) throw EmptyException("Demuxer");
    if (ret < 0) throw std::runtime_error("Demuxer: Failed to read a packet");
}

void Demuxer::dumpInfo() { av_dump_format(fmt_ctx_, 0, device_name_.c_str(), 0); }