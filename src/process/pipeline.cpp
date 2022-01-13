#include "pipeline.h"

#include <iostream>
#include <map>
#include <sstream>

#include "convert/audio_converter.h"
#include "convert/video_converter.h"
#include "encode/audio_encoder.h"
#include "encode/video_encoder.h"
#include "format/demuxer.h"
#include "format/muxer.h"

static void throw_error(const std::string &msg) { throw std::runtime_error("Pipeline: " + msg); }

static void checkDataType(av::DataType data_type) {
    if (!av::isDataTypeValid(data_type)) throw_error("invalid data type received");
}

Pipeline::Pipeline(std::shared_ptr<Muxer> muxer, bool use_background_processors)
    : muxer_(std::move(muxer)), use_background_processors_(use_background_processors) {
    if (!muxer_) throw_error("received Muxer is null");
    data_types_[av::DataType::Video] = false;
    data_types_[av::DataType::Audio] = false;
}

Pipeline::~Pipeline() {
    if (use_background_processors_) {
        stopProcessors();
        for (auto &p : processors_) {
            if (p.joinable()) p.join();
        }
    }
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

void Pipeline::initVideo(const Demuxer *demuxer, AVCodecID codec_id, const VideoParameters &video_params,
                         AVPixelFormat pix_fmt) {
    const auto type = av::DataType::Video;

    if (!demuxer) throw_error("received demuxer for video pipeline is null");

    if (data_types_[type]) throw_error("video pipeline already inited");
    data_types_[type] = true;

    /* Init decoder */
    decoders_[type] = std::make_unique<Decoder>(demuxer->getStreamParams(type));

    { /* Init encoder */
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
            std::make_unique<VideoEncoder>(codec_id, width, height, pix_fmt, demuxer->getStreamTimeBase(type),
                                           muxer_->getGlobalHeaderFlags(), enc_options);
    }

    /* Init converter */
    converters_[type] = std::make_unique<VideoConverter>(decoders_[type]->getContext(), encoders_[type]->getContext(),
                                                         demuxer->getStreamTimeBase(type), video_params.offset_x,
                                                         video_params.offset_y);

    muxer_->addStream(encoders_[type]->getContext(), type);

    if (use_background_processors_) startProcessor(type);
}

void Pipeline::initAudio(const Demuxer *demuxer, AVCodecID codec_id) {
    const auto type = av::DataType::Audio;

    if (!demuxer) throw_error("received demuxer for audio pipeline is null");

    if (data_types_[type]) throw_error("audio pipeline already inited");
    data_types_[type] = true;

    /* Init decoder */
    decoders_[type] = std::make_unique<Decoder>(demuxer->getStreamParams(type));

    { /* Init encoder */
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

    /* Init converter */
    converters_[type] = std::make_unique<AudioConverter>(decoders_[type]->getContext(), encoders_[type]->getContext(),
                                                         demuxer->getStreamTimeBase(type));

    muxer_->addStream(encoders_[type]->getContext(), type);

    if (use_background_processors_) startProcessor(type);
}

void Pipeline::processPacket(const AVPacket *packet, av::DataType data_type) {
    checkDataType(data_type);

    Decoder *decoder = decoders_[data_type].get();
    Converter *converter = converters_[data_type].get();

    if (!decoder) throw_error("no decoder present for the specified data type");
    if (!converter) throw_error("no decoder present for the specified data type");

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

    Encoder *encoder = encoders_[data_type].get();

    if (!encoder) throw_error("no decoder present for the specified data type");

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

void Pipeline::feed(av::PacketUPtr packet, av::DataType packet_type) {
    {
        std::unique_lock ul{m_};
        checkExceptions();
    }

    if (!packet) throw_error("received packet is null");
    checkDataType(packet_type);
    if (!data_types_[packet_type]) throw std::runtime_error("No pipeline corresponding to received packet type");

    if (use_background_processors_) {
        std::unique_lock ul{m_};
        if (!packets_[packet_type]) {  // if previous packet has been fully processed
            packets_[packet_type] = std::move(packet);
            packets_cv_[packet_type].notify_all();
        }
    } else {
        processPacket(packet.get(), packet_type);
    }
}

void Pipeline::flushPipeline(av::DataType data_type) {
    processPacket(nullptr, data_type);
    processConvertedFrame(nullptr, data_type);
}

void Pipeline::flush() {
    if (use_background_processors_) {
        /* stop all threads working on the pipelines */
        stopProcessors();
        for (auto &p : processors_) {
            if (p.joinable()) p.join();
        }
        checkExceptions();
    }

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
