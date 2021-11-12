#include "../include/demuxer.h"

Demuxer::Demuxer(const std::string &fmt_name, const std::string &device_name,
                 const std::map<std::string, std::string> &options)
    : fmt_ctx_(nullptr), device_name_(device_name), video_stream_(nullptr), audio_stream_(nullptr) {
    int ret;
    AVDictionary **dict_ptr = nullptr;

    AVInputFormat *fmt = av_find_input_format(fmt_name.c_str());
    if (!fmt) {
        throw std::runtime_error("Cannot find input format");
    }

    for (auto const &[key, val] : options) {
        if (av_dict_set(dict_ptr, key.c_str(), val.c_str(), 0) < 0) {
            if (*dict_ptr) av_dict_free(dict_ptr);
            throw std::runtime_error("Cannot set " + key + "in dictionary");
        }
    }

    ret = avformat_open_input(&fmt_ctx_, device_name_.c_str(), fmt, dict_ptr);
    if (*dict_ptr) av_dict_free(dict_ptr);
    /* TO-DO: free fmt (which function to use?) */
    if (ret) throw std::runtime_error("Cannot open input format");

    avformat_find_stream_info(fmt_ctx_, nullptr);

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
    if (fmt_ctx_) avformat_free_context(fmt_ctx_);  // This will also free the streams
}

int Demuxer::getVideoStreamIdx() { return video_stream_->index; };

int Demuxer::getAudioStreamIdx() { return audio_stream_->index; };

const AVCodecParameters *Demuxer::getVideoStreamParams() { return video_stream_->codecpar; }

const AVCodecParameters *Demuxer::getAudioStreamParams() { return audio_stream_->codecpar; }

AVPacket *Demuxer::readPacket() {
    AVPacket *packet = av_packet_alloc();
    if (!packet) throw std::runtime_error("Failed to allocate a packet");

    if (av_read_frame(fmt_ctx_, packet) < 0) {
        av_packet_free(&packet);
        throw std::runtime_error("Failed to read a packet");
    }

    return packet;
}

void Demuxer::dumpInfo() { av_dump_format(fmt_ctx_, 0, device_name_.c_str(), 0); }