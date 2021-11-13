#include "../include/muxer.h"

Muxer::Muxer(const std::string &filename)
    : fmt_ctx_(nullptr),
      filename_(filename),
      video_stream_(nullptr),
      audio_stream_(nullptr),
      file_opened_(false),
      file_closed_(false) {
    if (avformat_alloc_output_context2(&fmt_ctx_, NULL, NULL, filename_.c_str()) < 0)
        throw std::runtime_error("Failed to allocate output format context");
}

Muxer::~Muxer() {
    if (file_opened_ && !file_closed_) {
        std::cerr << "Demuxer: WARNING, the output file " << filename_
                  << " has not been closed, trying to close now...";
        if (avio_close(fmt_ctx_->pb) < 0) std::cerr << " failed to close file";
        std::cerr << std::endl;
    }
    if (fmt_ctx_) avformat_free_context(fmt_ctx_);  // This will also free the streams
}

void Muxer::addVideoStream(const AVCodecContext *codec_ctx) {
    if (file_opened_) throw std::runtime_error("Muxer: cannot add a new stream, file has already been opened");
    if (video_stream_) throw std::runtime_error("Muxer: Video stream already added");

    video_stream_ = avformat_new_stream(fmt_ctx_, NULL);
    if (!video_stream_) throw std::runtime_error("Muxer: Failed to create a new video stream");

    if (avcodec_parameters_from_context(video_stream_->codecpar, codec_ctx) < 0)
        throw std::runtime_error("Muxer: Failed to write video stream parameters");
}

void Muxer::addAudioStream(const AVCodecContext *codec_ctx) {
    if (file_opened_) throw std::runtime_error("Muxer: cannot add a new stream, file has already been opened");
    if (audio_stream_) throw std::runtime_error("Muxer: Audio stream already added");

    audio_stream_ = avformat_new_stream(fmt_ctx_, NULL);
    if (!audio_stream_) throw std::runtime_error("Muxer: Failed to create a new audio stream");

    if (avcodec_parameters_from_context(audio_stream_->codecpar, codec_ctx) < 0)
        throw std::runtime_error("Muxer: Failed to write audio stream parameters");
}

const AVStream *Muxer::getVideoStream() {
    if (!video_stream_) throw std::runtime_error("Muxer: Video stream not present");
    return video_stream_;
}

const AVStream *Muxer::getAudioStream() {
    if (!audio_stream_) throw std::runtime_error("Muxer: Audio stream not present");
    return audio_stream_;
}

void Muxer::openFile() {
    if (file_opened_) throw std::runtime_error("Muxer: cannot open file, file has already been opened");
    if (file_closed_) throw std::runtime_error("Muxer: cannot re-open file, file has already been closed");
    /* create empty video file */
    if (!(fmt_ctx_->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, filename_.c_str(), AVIO_FLAG_WRITE) < 0) {
            throw std::runtime_error("Muxer: Failed to create the output file");
        }
    }
    file_opened_ = true;
    if (avformat_write_header(fmt_ctx_, nullptr) < 0) throw std::runtime_error("Muxer: Failed to write file header");
}

void Muxer::closeFile() {
    if (!file_opened_) throw std::runtime_error("Muxer: cannot close file, file has not been opened");
    if (file_closed_) throw std::runtime_error("Muxer: cannot close file, file has already been closed");
    if (av_write_trailer(fmt_ctx_) < 0) throw std::runtime_error("Muxer: Failed to write file trailer");
    if (avio_close(fmt_ctx_->pb) < 0) throw std::runtime_error("Muxer: Failed to close file");
    file_closed_ = true;
}

void Muxer::writePacket(AVPacket *packet) {
    if (!file_opened_) throw std::runtime_error("Muxer: cannot write packet, file has not been opened");
    if (file_closed_) throw std::runtime_error("Muxer: cannot write packet, file has already been closed");
    if (av_interleaved_write_frame(fmt_ctx_, packet)) throw std::runtime_error("Muxer: Failed to write packet");
}

void Muxer::dumpInfo() { av_dump_format(fmt_ctx_, 0, filename_.c_str(), 1); }

int Muxer::getGlobalHeaderFlags() { return fmt_ctx_->oformat->flags; }