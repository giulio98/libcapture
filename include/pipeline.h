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
    std::shared_ptr<Muxer> muxer_;
    std::array<std::unique_ptr<Decoder>, av::DataType::NumDataTypes> decoders_;
    std::array<std::unique_ptr<Encoder>, av::DataType::NumDataTypes> encoders_;
    std::array<std::unique_ptr<Converter>, av::DataType::NumDataTypes> converters_;

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

    void initDecoder(const Demuxer *demuxer, av::DataType data_type);
    void addOutputStream(av::DataType data_type);

    void processPacket(const AVPacket *packet, av::DataType data_type);
    void processConvertedFrame(const AVFrame *frame, av::DataType data_type);
    void flushPipeline(av::DataType data_type);

public:
    /**
     * Create a new Pipeline for processing packets
     * @param muxer                     the muxer to send the processed packets to, for writing
     * @param use_background_processors whether the pipeline should use background threads to handle the processing
     * (recommended when the demuxer will provide both video and audio packets)
     */
    Pipeline(std::shared_ptr<Muxer> muxer, bool use_background_processors = false);

    ~Pipeline();

    /**
     * Initialize the video processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream
     * @param codec_id      the ID of the codec to use for the output video
     * @param video_params  the parameters to use for the output video
     * @param pix_fmt       the pixel format to use for the output video
     */
    void initVideo(const Demuxer *demuxer, AVCodecID codec_id, const VideoParameters &video_params,
                   AVPixelFormat pix_fmt);

    /**
     * Initialize the audio processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream
     * @param codec_id      the ID of the codec to use for the output audio
     */
    void initAudio(const Demuxer *demuxer, AVCodecID codec_id);

    /**
     * Send the packet to the processing chain corresponding to its type.
     * If 'use_background_processors' was set to true when building the Pipeline,
     * the background threads will handle the packet processing and this function will
     * return immediately, otherwise the processing will be handled in
     * a synchronous way and this function will return only when it's completed
     * @param packet        the packet to send to che processing chain
     * @param packet_type   the type of the packet to process
     */
    void feed(av::PacketUPtr packet, av::DataType packet_type);

    /**
     * Flush the processing pipelines.
     * If 'use_background_processors' was set to true when building the Pipeline,
     * the background threads will be completely stopped before the actual flushing.
     * WARNING: this function must be called BEFORE closing the output file with Muxer::closeFile,
     * otherwise some packets/frames will be left in the processing structures and eventual background workers
     * won't be able to write the packets to he output file, throwing an exception
     */
    void flush();

    /**
     * Print the informations about the internal demuxer, decoders and encoders
     */
    void printInfo() const;
};