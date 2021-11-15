#include "../include/muxer.h"

Muxer::Muxer(const std::string &filename)
    : fmt_ctx_(nullptr),
      filename_(filename),
      video_stream_(nullptr),
      audio_stream_(nullptr),
      file_opened_(false),
      file_closed_(false),
      time_base_valid_(false) {
    try {
        if (avformat_alloc_output_context2(&fmt_ctx_, NULL, NULL, filename_.c_str()) < 0)
            throw std::runtime_error("Failed to allocate output format context");
    } catch (const std::exception &e) {
        cleanup();
        throw;
    }
}

Muxer::~Muxer() {
    if (file_opened_ && !file_closed_) {
        std::cerr << "Demuxer: WARNING, the output file " << filename_
                  << " has not been closed, trying to close now...";
        if (avio_close(fmt_ctx_->pb) < 0) std::cerr << " failed to close file";
        std::cerr << std::endl;
    }
    cleanup();
}

void Muxer::cleanup() {
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

const AVCodecParameters *Muxer::getVideoParams() const {
    if (!video_stream_) throw std::runtime_error("Muxer: Video stream not present");
    return video_stream_->codecpar;
}

const AVCodecParameters *Muxer::getAudioParams() const {
    if (!audio_stream_) throw std::runtime_error("Muxer: Audio stream not present");
    return audio_stream_->codecpar;
}

AVRational Muxer::getVideoTimeBase() const {
    if (!video_stream_) throw std::runtime_error("Muxer: Video stream not present");
    if (!time_base_valid_)
        throw std::runtime_error("Muxer: Time base has not been set yet, open the file with openFile() first");
    return video_stream_->time_base;
}

AVRational Muxer::getAudioTimeBase() const {
    if (!audio_stream_) throw std::runtime_error("Muxer: Audio stream not present");
    if (!time_base_valid_)
        throw std::runtime_error("Muxer: Time base has not been set yet, open the file with openFile() first");
    return audio_stream_->time_base;
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
    time_base_valid_ = true;  // now the time_bases are set
}

void Muxer::closeFile() {
    if (!file_opened_) throw std::runtime_error("Muxer: cannot close file, file has not been opened");
    if (file_closed_) throw std::runtime_error("Muxer: cannot close file, file has already been closed");
    if (av_write_trailer(fmt_ctx_) < 0) throw std::runtime_error("Muxer: Failed to write file trailer");
    if (avio_close(fmt_ctx_->pb) < 0) throw std::runtime_error("Muxer: Failed to close file");
    file_closed_ = true;
}

void Muxer::writePacket(std::shared_ptr<AVPacket> packet, AVType packet_type) const {
    if (!file_opened_) throw std::runtime_error("Muxer: cannot write packet, file has not been opened");
    if (file_closed_) throw std::runtime_error("Muxer: cannot write packet, file has already been closed");

    if (packet) {
        if (packet_type == video) {
            if (!video_stream_) throw std::runtime_error("Muxer: Video stream not present");
            packet->stream_index = video_stream_->index;
        } else if (packet_type == audio) {
            if (!audio_stream_) throw std::runtime_error("Muxer: Audio stream not present");
            packet->stream_index = audio_stream_->index;
        } else {
            throw std::runtime_error("Muxer: received packet is of unknown type");
        }
    }

    if (av_interleaved_write_frame(fmt_ctx_, packet.get())) throw std::runtime_error("Muxer: Failed to write packet");
}

void Muxer::dumpInfo() const { av_dump_format(fmt_ctx_, 0, filename_.c_str(), 1); }

int Muxer::getGlobalHeaderFlags() const { return fmt_ctx_->oformat->flags; }