#include "../include/input_container.h"

InputContainer::InputContainer(const std::string &fmt_name, const std::string &device_name)
    : fmt_ctx_(nullptr),
      fmt_(nullptr),
      device_name_(device_name),
      options_(nullptr),
      video_stream_(nullptr),
      audio_stream_(nullptr) {
    fmt_ = av_find_input_format(fmt_name.c_str());
}

InputContainer::~InputContainer() {
    if (fmt_ctx_) avformat_free_context(fmt_ctx_);  // This will also free the streams
    if (options_) av_dict_free(&options_);
    /* TO-DO: free fmt_ (which function to use?) */
}

void InputContainer::setOption(const std::string &key, const std::string &value) {
    av_dict_set(&options_, key.c_str(), value.c_str(), 0);
}

void InputContainer::open() {
    avformat_open_input(&fmt_ctx_, device_name_.c_str(), fmt_, &options_);

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

void InputContainer::dumpInfo() { av_dump_format(fmt_ctx_, 0, device_name_.c_str(), 0); }