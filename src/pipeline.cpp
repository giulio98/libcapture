#include "pipeline.h"

#include <map>
#include <sstream>

Pipeline::Pipeline(std::shared_ptr<Demuxer> demuxer, std::shared_ptr<Muxer> muxer)
    : demuxer_(demuxer), muxer_(muxer), pts_offset_(0), last_pts_(0), adjust_pts_offset_(false) {
    if (!demuxer_) throw std::runtime_error("Demuxer is NULL");
    if (!muxer_) throw std::runtime_error("Muxer is NULL");
    data_types_[av::DataType::Video] = false;
    data_types_[av::DataType::Audio] = false;
}

Pipeline::~Pipeline() {
    stopAndNotify();
    for (auto &t : processors_) {
        if (t.joinable()) t.join();
    }
}

void Pipeline::stopAndNotify() {
    std::unique_lock<std::mutex> ul{m_};
    stop_ = true;
    for (auto &cv : packets_cv_) cv.notify_all();
}

void Pipeline::checkExceptions() {
    for (auto &e_ptr : e_ptrs_) {
        if (e_ptr) std::rethrow_exception(e_ptr);
    }
}

void Pipeline::startProcessor(av::DataType data_type) {
    if (processors_[data_type].joinable()) processors_[data_type].join();

    processors_[data_type] = std::thread([this, data_type]() {
        try {
            if (!av::isDataTypeValid(data_type))
                throw std::runtime_error("Invalid packet type specified for processing");
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
            e_ptrs_[data_type] = std::current_exception();
            stopAndNotify();
        }
    });
}

void Pipeline::initDecoder(av::DataType data_type) {
    // TO-DO
}

void Pipeline::addOutputStream(av::DataType data_type) {
    muxer_->addStream(encoders_[data_type]->getContext(), data_type);
}

void Pipeline::initVideo(AVCodecID codec_id, const VideoDimensions &video_dims, AVPixelFormat pix_fmt) {
    data_types_[av::DataType::Video] = true;

    decoders_[av::DataType::Video] = std::make_unique<Decoder>(demuxer_->getVideoParams());

    {
        auto dec_ctx = decoders_[av::DataType::Video]->getContext();
        int width = (video_dims.width) ? video_dims.width : dec_ctx->width;
        int height = (video_dims.height) ? video_dims.height : dec_ctx->height;

        if (video_dims.offset_x + width > dec_ctx->width) throw std::runtime_error("Total width exceeds display");
        if (video_dims.offset_y + height > dec_ctx->height) throw std::runtime_error("Total height exceeds display");

        std::map<std::string, std::string> enc_options;
        enc_options.insert({"preset", "ultrafast"});

        encoders_[av::DataType::Video] =
            std::make_unique<VideoEncoder>(codec_id, width, height, pix_fmt, demuxer_->getVideoTimeBase(),
                                           muxer_->getGlobalHeaderFlags(), enc_options);
    }

    converters_[av::DataType::Video] = std::make_unique<VideoConverter>(
        decoders_[av::DataType::Video]->getContext(), encoders_[av::DataType::Video]->getContext(),
        demuxer_->getVideoTimeBase(), video_dims.offset_x, video_dims.offset_y);

    addOutputStream(av::DataType::Video);
    startProcessor(av::DataType::Video);
}

void Pipeline::initAudio(AVCodecID codec_id) {
    data_types_[av::DataType::Audio] = true;

    decoders_[av::DataType::Audio] = std::make_unique<Decoder>(demuxer_->getAudioParams());

    {
        const AVCodecContext *dec_ctx = decoders_[av::DataType::Audio]->getContext();
        auto channel_layout =
            (dec_ctx->channel_layout) ? dec_ctx->channel_layout : av_get_default_channel_layout(dec_ctx->channels);

        std::map<std::string, std::string> enc_options;

        encoders_[av::DataType::Audio] = std::make_unique<AudioEncoder>(codec_id, dec_ctx->sample_rate, channel_layout,
                                                                        muxer_->getGlobalHeaderFlags(), enc_options);
    }

    converters_[av::DataType::Audio] =
        std::make_unique<AudioConverter>(decoders_[av::DataType::Audio]->getContext(),
                                         encoders_[av::DataType::Audio]->getContext(), demuxer_->getAudioTimeBase());

    addOutputStream(av::DataType::Audio);
    startProcessor(av::DataType::Audio);
}

void Pipeline::processPacket(const AVPacket *packet, av::DataType data_type) {
    if (!av::isDataTypeValid(data_type)) throw std::runtime_error("Invalid packet received for processing");

    const Decoder *decoder = decoders_[data_type].get();
    const Converter *converter = converters_[data_type].get();

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
    if (!av::isDataTypeValid(data_type)) throw std::runtime_error("Invalid frame received for processing");

    const Encoder *encoder = encoders_[data_type].get();

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
    checkExceptions();

    auto [packet, packet_type] = demuxer_->readPacket();
    if (!packet) return false;

    if (!av::isDataTypeValid(packet_type)) throw std::runtime_error("Invalid packet received from demuxer");
    if (!data_types_[packet_type]) throw std::runtime_error("No pipeline corresponding to received packet type");

    if (recovering_from_pause) pts_offset_ += (packet->pts - last_pts_);
    last_pts_ = packet->pts;

    if (!recovering_from_pause) {
        std::unique_lock ul{m_};
        if (!packets_[packet_type]) {  // if previous packet has been fully processed
            packets_[packet_type] = std::move(packet);
            packets_cv_[packet_type].notify_all();
        }
    }

    return true;
}

void Pipeline::flushPipeline(av::DataType data_type) {
    processPacket(nullptr, data_type);
    processConvertedFrame(nullptr, data_type);
}

void Pipeline::flush() {
    /* stop all threads working on the pipelines */
    stopAndNotify();
    for (auto &t : processors_)
        if (t.joinable()) t.join();
    checkExceptions();

    /* flush the pipelines */
    for (auto type : {av::DataType::Video, av::DataType::Audio}) {
        if (data_types_[type]) flushPipeline(type);
    }
}