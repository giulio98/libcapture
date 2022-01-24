#include "pipeline.h"

#include <assert.h>

#include <iostream>
#include <map>

#include "format/demuxer.h"
#include "format/muxer.h"

static void throwRuntimeError(const std::string &msg) { throw std::runtime_error("Pipeline: " + msg); }
static void throwLogicError(const std::string &msg) { throw std::logic_error("Pipeline: " + msg); }

Pipeline::Pipeline(const std::string &output_file, const bool async) : muxer_(output_file), async_(async) {}

Pipeline::~Pipeline() {
    if (async_) stopProcessors();
}

void Pipeline::startProcessor(const av::MediaType type) {
    assert(av::validMediaType(type));
    assert(managed_types_[type]);
    assert(!processors_[type].joinable());

    processors_[type] = std::thread([this, type]() {
        try {
            while (true) {
                av::PacketUPtr packet;
                {
                    std::unique_lock ul(processors_m_);
                    packets_cv_[type].wait(ul, [this, type]() { return (packets_[type] || stopped_); });
                    if (!packets_[type] && stopped_) break;
                    packet = std::move(packets_[type]);
                }
                processPacket(packet.get(), type);
            }
        } catch (...) {
            std::lock_guard lg(processors_m_);
            e_ptrs_[type] = std::current_exception();
        }
    });
}

void Pipeline::stopProcessors() {
    {
        std::lock_guard lg(processors_m_);
        stopped_ = true;
        for (auto &cv : packets_cv_) cv.notify_all();
    }
    for (auto &p : processors_) {
        if (p.joinable()) p.join();
    }
}

void Pipeline::checkExceptions() {
    for (auto &e_ptr : e_ptrs_) {
        if (e_ptr) std::rethrow_exception(e_ptr);
    }
}

void Pipeline::initVideo(const Demuxer &demuxer, const AVCodecID codec_id, const AVPixelFormat pix_fmt,
                         const VideoParameters &video_params) {
    const auto type = av::MediaType::Video;

    if (muxer_.isInited()) throwLogicError("output has already been initialized");
    if (managed_types_[type]) throwLogicError("video pipeline already initialized");

    managed_types_[type] = true;

    /* Init decoder */
    decoders_[type] = Decoder(demuxer.getStreamParams(type));

    auto dec_ctx = decoders_[type].getContext();
    auto [width, height] = video_params.getVideoSize();
    auto [offset_x, offset_y] = video_params.getVideoOffset();
    if (!width) width = dec_ctx->width;
    if (!height) height = dec_ctx->height;
    if (offset_x + width > dec_ctx->width) throwRuntimeError("Output video width exceeds input one");
    if (offset_y + height > dec_ctx->height) throwRuntimeError("Output video height exceeds input one");

    /* Init encoder */
    std::map<std::string, std::string> enc_options;
    /*
     * Possible presets from fastest (and worst quality) to slowest (and best quality):
     * ultrafast -> superfast -> veryfast -> faster -> fast -> medium
     */
    enc_options.insert({"preset", "ultrafast"});
    encoders_[type] = Encoder(codec_id, width, height, pix_fmt, demuxer.getStreamTimeBase(type),
                              muxer_.getGlobalHeaderFlags(), enc_options);

    /* Init converter */
    converters_[type] = Converter(decoders_[type].getContext(), encoders_[type].getContext(),
                                  demuxer.getStreamTimeBase(type), offset_x, offset_y);

    muxer_.addStream(encoders_[type].getContext());

    if (async_) startProcessor(type);
}

void Pipeline::initAudio(const Demuxer &demuxer, const AVCodecID codec_id) {
    const auto type = av::MediaType::Audio;

    if (muxer_.isInited()) throwLogicError("output has already been initialized");
    if (managed_types_[type]) throwLogicError("audio pipeline already initialized");

    managed_types_[type] = true;

    /* Init decoder */
    decoders_[type] = Decoder(demuxer.getStreamParams(type));

    auto dec_ctx = decoders_[type].getContext();
    uint64_t channel_layout;
    if (dec_ctx->channel_layout) {
        channel_layout = dec_ctx->channel_layout;
    } else {
        channel_layout = av_get_default_channel_layout(dec_ctx->channels);
    }

    /* Init encoder */
    encoders_[type] = Encoder(codec_id, dec_ctx->sample_rate, channel_layout, muxer_.getGlobalHeaderFlags(),
                              std::map<std::string, std::string>());

    /* Init converter */
    converters_[type] =
        Converter(decoders_[type].getContext(), encoders_[type].getContext(), demuxer.getStreamTimeBase(type));

    muxer_.addStream(encoders_[type].getContext());

    if (async_) startProcessor(type);
}

void Pipeline::initOutput() {
    if (muxer_.isInited()) throwLogicError("output has already been initialized");
    muxer_.openFile();
}

void Pipeline::processPacket(const AVPacket *packet, const av::MediaType type) {
    assert(av::validMediaType(type));
    assert(managed_types_[type]);

    Decoder &decoder = decoders_[type];
    Converter &converter = converters_[type];

    bool decoder_received = false;
    while (!decoder_received) {
        decoder_received = decoder.sendPacket(packet);

        while (true) {
            auto frame = decoder.getFrame();
            if (!frame) break;
            converter.sendFrame(std::move(frame));

            while (true) {
                auto converted_frame = converter.getFrame();
                if (!converted_frame) break;
                processConvertedFrame(converted_frame.get(), type);
            }
        }
    }
}

void Pipeline::processConvertedFrame(const AVFrame *frame, const av::MediaType type) {
    assert(av::validMediaType(type));
    assert(managed_types_[type]);

    Encoder &encoder = encoders_[type];

    bool encoder_received = false;
    while (!encoder_received) {
        encoder_received = encoder.sendFrame(frame);

        while (true) {
            auto packet = encoder.getPacket();
            if (!packet) break;
            std::lock_guard lg(muxer_m_);
            muxer_.writePacket(std::move(packet), type);
        }
    }
}

void Pipeline::feed(av::PacketUPtr packet, const av::MediaType packet_type) {
    if (!muxer_.isInited()) throwLogicError("the output file hasn't been initialized yet");
    if (!packet) throwLogicError("received packet is null");
    if (!av::validMediaType(packet_type)) throwLogicError("received media type is invalid");
    if (!managed_types_[packet_type]) throwLogicError("received media type is not handled by the pipeline");

    if (async_) {
        std::lock_guard lg(processors_m_);
        if (stopped_) throwLogicError("processors have been stopped");
        checkExceptions();
        if (!packets_[packet_type]) {  // if previous packet has been fully processed
            packets_[packet_type] = std::move(packet);
            packets_cv_[packet_type].notify_all();
        }
    } else {
        processPacket(packet.get(), packet_type);
    }
}

void Pipeline::flushPipeline(const av::MediaType type) {
    processPacket(nullptr, type);
    processConvertedFrame(nullptr, type);
}

void Pipeline::flush() {
    // TO-DO: handle exceptions on multiple calls

    if (async_) {
        /* stop all threads working on the pipelines */
        stopProcessors();
        checkExceptions();
    }

    /* flush the pipelines */
    for (auto type : av::validMediaTypes) {
        if (managed_types_[type]) flushPipeline(type);
    }

    muxer_.writePacket(nullptr, av::MediaType::None);
    muxer_.closeFile();
}

void Pipeline::printInfo() const {
    for (auto type : av::validMediaTypes) {
        if (managed_types_[type]) {
            std::cout << "Decoder " << type << ": " << decoders_[type].getName() << std::endl;
            std::cout << "Encoder " << type << ": " << encoders_[type].getName() << std::endl;
        }
    }
}
