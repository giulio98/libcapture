#include "pipeline.h"

#include <iostream>
#include <map>
#include <sstream>

#define USE_PROCESSING_THREADS 0

static void throw_error(const std::string &msg) { throw std::runtime_error("Pipeline: " + msg); }

static void checkDataType(av::DataType data_type) {
    if (!av::isDataTypeValid(data_type)) throw_error("invalid data type received");
}

Pipeline::Pipeline(std::shared_ptr<Demuxer> demuxer, std::shared_ptr<Muxer> muxer)
    : demuxer_(demuxer), muxer_(muxer), pts_offset_(0), last_pts_(0) {
    if (!demuxer_) throw std::runtime_error("Demuxer is NULL");
    if (!muxer_) throw std::runtime_error("Muxer is NULL");
    data_types_[av::DataType::Video] = false;
    data_types_[av::DataType::Audio] = false;
}

Pipeline::~Pipeline() {
#if USE_PROCESSING_THREADS
    stopProcessors();
    for (auto &p : processors_) {
        if (p.joinable()) p.join();
    }
#endif
}

void Pipeline::startProcessor(av::DataType data_type) {
    checkDataType(data_type);

    if (processors_[data_type].joinable()) processors_[data_type].join();

    processors_[data_type] = std::thread([this, data_type]() {
        try {
            while (true) {
                av::PacketUPtr packet;
                {
                    std::unique_lock ul{m_};
                    packets_cv_[data_type].wait(ul, [this, data_type]() { return (packets_[data_type] || stop_); });
                    if (!packets_[data_type] && stop_) break;
                    packet = std::move(packets_[data_type]);
                }
                processPacket(packet.get(), data_type);
            }
        } catch (...) {
            {
                std::unique_lock ul{m_};
                e_ptrs_[data_type] = std::current_exception();
            }
            stopProcessors();
        }
    });
}

void Pipeline::stopProcessors() {
    std::unique_lock<std::mutex> ul{m_};
    stop_ = true;
    for (auto &cv : packets_cv_) cv.notify_all();
}

void Pipeline::checkExceptions() {
    for (auto &e_ptr : e_ptrs_) {
        if (e_ptr) std::rethrow_exception(e_ptr);
    }
}

void Pipeline::initDecoder(av::DataType data_type) {
    checkDataType(data_type);
    if (decoders_[data_type]) throw_error("decoder already present");
    decoders_[data_type] = std::make_unique<Decoder>(demuxer_->getStreamParams(data_type));
}

void Pipeline::addOutputStream(av::DataType data_type) {
    checkDataType(data_type);
    if (!encoders_[data_type]) throw_error("no encoder for the specified data type");
    muxer_->addStream(encoders_[data_type]->getContext(), data_type);
}

void Pipeline::initVideo(AVCodecID codec_id, const VideoParameters &video_params, AVPixelFormat pix_fmt) {
    const auto type = av::DataType::Video;

    if (data_types_[type]) throw_error("video pipeline already inited");
    data_types_[type] = true;

    initDecoder(type);

    {
        auto dec_ctx = decoders_[type]->getContext();
        int width = (video_params.width) ? video_params.width : dec_ctx->width;
        int height = (video_params.height) ? video_params.height : dec_ctx->height;
        if (video_params.offset_x + width > dec_ctx->width)
            throw std::runtime_error("Output video width exceeds input one");
        if (video_params.offset_y + height > dec_ctx->height)
            throw std::runtime_error("Output video height exceeds input one");

        std::map<std::string, std::string> enc_options;
        /*
         * Possible presets from fastest (and worst quality) to slowest (and best quality):
         * ultrafast -> superfast -> veryfast -> faster -> fast -> medium
         */
        enc_options.insert({"preset", "ultrafast"});

        encoders_[type] =
            std::make_unique<VideoEncoder>(codec_id, width, height, pix_fmt, demuxer_->getStreamTimeBase(type),
                                           muxer_->getGlobalHeaderFlags(), enc_options);
    }

    converters_[type] = std::make_unique<VideoConverter>(decoders_[type]->getContext(), encoders_[type]->getContext(),
                                                         demuxer_->getStreamTimeBase(type), video_params.offset_x,
                                                         video_params.offset_y);

    addOutputStream(type);
#if USE_PROCESSING_THREADS
    startProcessor(type);
#endif
}

void Pipeline::initAudio(AVCodecID codec_id) {
    const auto type = av::DataType::Audio;

    if (data_types_[type]) throw std::runtime_error("Audio pipeline already inited");
    data_types_[type] = true;

    initDecoder(type);

    {
        auto dec_ctx = decoders_[type]->getContext();
        uint64_t channel_layout;
        if (dec_ctx->channel_layout) {
            channel_layout = dec_ctx->channel_layout;
        } else {
            channel_layout = av_get_default_channel_layout(dec_ctx->channels);
        }

        std::map<std::string, std::string> enc_options;

        encoders_[type] = std::make_unique<AudioEncoder>(codec_id, dec_ctx->sample_rate, channel_layout,
                                                         muxer_->getGlobalHeaderFlags(), enc_options);
    }

    converters_[type] = std::make_unique<AudioConverter>(decoders_[type]->getContext(), encoders_[type]->getContext(),
                                                         demuxer_->getStreamTimeBase(type));

    addOutputStream(type);
#if USE_PROCESSING_THREADS
    startProcessor(type);
#endif
}

void Pipeline::processPacket(const AVPacket *packet, av::DataType data_type) {
    checkDataType(data_type);

    const Decoder *decoder = decoders_[data_type].get();
    const Converter *converter = converters_[data_type].get();

    if (!decoder) throw_error("no decoder prsent for the specified data type");
    if (!converter) throw_error("no decoder prsent for the specified data type");

    bool decoder_received = false;
    while (!decoder_received) {
        decoder_received = decoder->sendPacket(packet);

        while (true) {
            auto frame = decoder->getFrame();
            if (!frame) break;
            converter->sendFrame(std::move(frame));

            while (true) {
                auto converted_frame = converter->getFrame();
                if (!converted_frame) break;
                processConvertedFrame(converted_frame.get(), data_type);
            }
        }
    }
}

void Pipeline::processConvertedFrame(const AVFrame *frame, av::DataType data_type) {
    checkDataType(data_type);

    const Encoder *encoder = encoders_[data_type].get();

    if (!encoder) throw_error("no decoder prsent for the specified data type");

    bool encoder_received = false;
    while (!encoder_received) {
        encoder_received = encoder->sendFrame(frame);

        while (true) {
            auto packet = encoder->getPacket();
            if (!packet) break;
            muxer_->writePacket(std::move(packet), data_type);
        }
    }
}

bool Pipeline::step(bool recovering_from_pause) {
    {
        std::unique_lock ul{m_};
        checkExceptions();
    }

    auto [packet, packet_type] = demuxer_->readPacket();
    if (!packet) return false;

    checkDataType(packet_type);
    if (!data_types_[packet_type]) throw std::runtime_error("No pipeline corresponding to received packet type");

    if (recovering_from_pause) pts_offset_ += (packet->pts - last_pts_);

    last_pts_ = packet->pts;

    if (!recovering_from_pause) {
        packet->pts -= pts_offset_;
#if USE_PROCESSING_THREADS
        std::unique_lock ul{m_};
        if (!packets_[packet_type]) {  // if previous packet has been fully processed
            packets_[packet_type] = std::move(packet);
            packets_cv_[packet_type].notify_all();
        }
#else
        processPacket(packet.get(), packet_type);
#endif
    }

    return true;
}

void Pipeline::flushPipeline(av::DataType data_type) {
    processPacket(nullptr, data_type);
    processConvertedFrame(nullptr, data_type);
}

void Pipeline::flush() {
#if USE_PROCESSING_THREADS
    /* stop all threads working on the pipelines */
    stopProcessors();
    for (auto &p : processors_) {
        if (p.joinable()) p.join();
    }
    checkExceptions();
#endif

    /* flush the pipelines */
    for (auto type : {av::DataType::Video, av::DataType::Audio}) {
        if (data_types_[type]) flushPipeline(type);
    }
}

void Pipeline::printInfo() const {
    for (auto type : {av::DataType::Video, av::DataType::Audio}) {
        if (decoders_[type]) std::cout << "Decoder " << type << ": " << decoders_[type]->getName() << std::endl;
        if (encoders_[type]) std::cout << "Encoder " << type << ": " << encoders_[type]->getName() << std::endl;
    }
}
