#pragma once

#include <array>
#include <condition_variable>
#include <vector>

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
    std::array<bool, av::DataType::NumDataTypes> data_types_;
    std::unique_ptr<Demuxer> demuxer_;
    std::shared_ptr<Muxer> muxer_;
    std::array<std::unique_ptr<Decoder>, av::DataType::NumDataTypes> decoders_;
    std::array<std::unique_ptr<Encoder>, av::DataType::NumDataTypes> encoders_;
    std::array<std::unique_ptr<Converter>, av::DataType::NumDataTypes> converters_;

    int64_t pts_offset_;
    int64_t last_pts_;

    bool use_background_processors_;
    std::mutex m_;
    bool stop_;
    std::array<std::thread, av::DataType::NumDataTypes> processors_;
    std::array<av::PacketUPtr, av::DataType::NumDataTypes> packets_;
    std::array<std::condition_variable, av::DataType::NumDataTypes> packets_cv_;
    std::array<std::exception_ptr, av::DataType::NumDataTypes> e_ptrs_;
    void startProcessor(av::DataType data_type);
    void stopProcessors();
    void checkExceptions();

    void initDecoder(av::DataType data_type);
    void addOutputStream(av::DataType data_type);

    void processPacket(const AVPacket *packet, av::DataType data_type);
    void processConvertedFrame(const AVFrame *frame, av::DataType data_type);
    void flushPipeline(av::DataType data_type);

public:
    Pipeline(std::unique_ptr<Demuxer> demuxer, std::shared_ptr<Muxer> muxer, bool use_background_processors = false);

    ~Pipeline();

    void initVideo(AVCodecID codec_id, const VideoParameters &video_params, AVPixelFormat pix_fmt);

    void initAudio(AVCodecID codec_id);

    bool step(bool recovering_from_pause = false);

    void flush();

    void printInfo() const;
};