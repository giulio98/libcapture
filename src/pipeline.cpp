#include "pipeline.h"

#include <map>
#include <sstream>

Pipeline::Pipeline(std::shared_ptr<Demuxer> demuxer, std::shared_ptr<Muxer> muxer)
    : demuxer_(demuxer), muxer_(muxer), video_(false), audio_(false) {
    if (!demuxer_) throw std::runtime_error("Demuxer is NULL");
    if (!muxer_) throw std::runtime_error("Muxer is NULL");
}

void Pipeline::addDecoder(av::DataType data_type) {
    // TO-DO
}

void Pipeline::addOutputStream(av::DataType data_type) {
    muxer_->addStream(encoders_[data_type]->getContext(), data_type);
}

void Pipeline::initVideo(AVCodecID codec_id, AVPixelFormat pix_fmt, const VideoParameters &video_params) {
    video_ = true;

    decoders_[av::DataType::Video] = std::make_unique<Decoder>(demuxer_->getVideoParams());

    {
        std::map<std::string, std::string> enc_options;
        enc_options.insert({"preset", "ultrafast"});
        encoders_[av::DataType::Video] =
            std::make_unique<VideoEncoder>(codec_id, video_params.width_, video_params.height_, pix_fmt,
                                           demuxer_->getVideoTimeBase(), muxer_->getGlobalHeaderFlags(), enc_options);
    }

    addOutputStream(av::DataType::Video);

    converters_[av::DataType::Video] = std::make_unique<VideoConverter>(
        decoders_[av::DataType::Video]->getContext(), encoders_[av::DataType::Video]->getContext(),
        demuxer_->getVideoTimeBase(), video_params.offset_x_, video_params.offset_y_);
}

void Pipeline::initAudio(AVCodecID codec_id) {
    audio_ = true;

    decoders_[av::DataType::Audio] = std::make_unique<Decoder>(demuxer_->getVideoParams());

    {
        std::map<std::string, std::string> enc_options;

        const AVCodecContext *dec_ctx = decoders_[av::DataType::Audio]->getContext();
        auto channel_layout =
            (dec_ctx->channel_layout) ? dec_ctx->channel_layout : av_get_default_channel_layout(dec_ctx->channels);

        encoders_[av::DataType::Audio] = std::make_unique<AudioEncoder>(codec_id, dec_ctx->sample_rate, channel_layout,
                                                                        muxer_->getGlobalHeaderFlags(), enc_options);
    }

    addOutputStream(av::DataType::Audio);

    converters_[av::DataType::Audio] =
        std::make_unique<AudioConverter>(decoders_[av::DataType::Audio]->getContext(),
                                         encoders_[av::DataType::Audio]->getContext(), demuxer_->getAudioTimeBase());
}