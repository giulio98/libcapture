#pragma once

#include <array>
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
#include "video_dimensions.h"
#include "video_encoder.h"

class Pipeline {
    std::array<bool, av::DataType::NumDataTypes> data_types_;
    std::shared_ptr<Demuxer> demuxer_;
    std::shared_ptr<Muxer> muxer_;
    // TO-DO: make these automatic objects rather than pointers
    std::array<std::unique_ptr<Decoder>, av::DataType::NumDataTypes> decoders_;
    std::array<std::unique_ptr<Encoder>, av::DataType::NumDataTypes> encoders_;
    std::array<std::unique_ptr<Converter>, av::DataType::NumDataTypes> converters_;

    int64_t pts_offset_;
    int64_t last_pts_;

    std::mutex m_;
    bool stop_;
    std::array<std::thread, av::DataType::NumDataTypes> processors_;
    std::array<av::PacketUPtr, av::DataType::NumDataTypes> packets_;
    std::array<std::condition_variable, av::DataType::NumDataTypes> packets_cv_;
    std::array<std::exception_ptr, av::DataType::NumDataTypes> e_ptrs_;
    void startProcessor(av::DataType data_type);
    void stop();
    void checkExceptions();

    void initDecoder(av::DataType data_type);
    void addOutputStream(av::DataType data_type);

    void processPacket(const AVPacket *packet, av::DataType data_type);
    void processConvertedFrame(const AVFrame *frame, av::DataType data_type);
    void flushPipeline(av::DataType data_type);

public:
    Pipeline(std::shared_ptr<Demuxer> demuxer, std::shared_ptr<Muxer> muxer);

    ~Pipeline();

    void initVideo(AVCodecID codec_id, const VideoDimensions &video_dims, AVPixelFormat pix_fmt);

    void initAudio(AVCodecID codec_id);

    bool step(bool recovering_from_pause = false);

    void flush();

    void printInfo() const;
};