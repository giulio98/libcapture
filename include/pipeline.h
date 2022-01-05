#pragma once

#include "audio_converter.h"
#include "audio_encoder.h"
#include "common.h"
#include "decoder.h"
#include "demuxer.h"
#include "muxer.h"
#include "thread"
#include "thread_guard.h"
#include "video_converter.h"
#include "video_encoder.h"
#include "video_parameters.h"

class Pipeline {
    bool video_;
    bool audio_;
    std::shared_ptr<Demuxer> demuxer_;
    std::shared_ptr<Muxer> muxer_;
    std::array<std::unique_ptr<Decoder>, av::DataType::NumDataTypes> decoders_;
    std::array<std::unique_ptr<Encoder>, av::DataType::NumDataTypes> encoders_;
    std::array<std::unique_ptr<Converter>, av::DataType::NumDataTypes> converters_;
    std::map<std::string, std::string> video_encoder_options_;

#ifndef LINUX
    std::thread video_processor_;
    std::thread audio_processor_;
    std::array<av::PacketUPtr, av::DataType::NumDataTypes> packets_;
    std::array<std::condition_variable, av::DataType::NumDataTypes> packets_cv_;
    void process(av::DataType data_type, std::exception_ptr &e_ptr);
#endif

    void addDecoder(av::DataType data_type);
    void addOutputStream(av::DataType data_type);

public:
    Pipeline(std::shared_ptr<Demuxer> demuxer, std::shared_ptr<Muxer> muxer);

    ~Pipeline();

    void initVideo(AVCodecID codec_id, AVPixelFormat pix_fmt, const VideoParameters &video_params);

    void initAudio(AVCodecID codec_id);

    bool step();
};