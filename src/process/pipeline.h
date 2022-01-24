#pragma once

#include <array>
#include <condition_variable>
#include <thread>
#include <vector>

#include "common/common.h"
#include "converter.h"
#include "decoder.h"
#include "encoder.h"
#include "format/muxer.h"
#include "video_parameters.h"

class Demuxer;

class Pipeline {
    const bool async_;

    std::array<bool, av::MediaType::NumTypes> managed_types_{};
    std::array<Decoder, av::MediaType::NumTypes> decoders_;
    std::array<Encoder, av::MediaType::NumTypes> encoders_;
    std::array<Converter, av::MediaType::NumTypes> converters_;
    Muxer muxer_;
    std::mutex muxer_m_;

    std::mutex processors_m_;
    bool stopped_{};
    std::array<std::thread, av::MediaType::NumTypes> processors_;
    std::array<av::PacketUPtr, av::MediaType::NumTypes> packets_;
    std::array<std::condition_variable, av::MediaType::NumTypes> packets_cv_;
    std::array<std::exception_ptr, av::MediaType::NumTypes> e_ptrs_;
    void startProcessor(av::MediaType media_type);
    void stopProcessors();
    void checkExceptions();

    void processPacket(const AVPacket *packet, av::MediaType type);
    void processConvertedFrame(const AVFrame *frame, av::MediaType type);
    void flushPipeline(av::MediaType type);

public:
    /**
     * Create a new Pipeline for processing packets
     * @param output_file   the name of the output file
     * @param async         whether the pipeline should use background threads to handle the processing
     * (recommended when a single demuxer will provide both video and audio packets)
     */
    explicit Pipeline(const std::string &output_file, bool async = false);

    Pipeline(const Pipeline &) = delete;

    ~Pipeline();

    Pipeline &operator=(const Pipeline &) = delete;

    /**
     * Initialize the video processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream of packets
     * @param codec_id      the ID of the codec to use for the output video
     * @param pix_fmt       the pixel format to use for the output video
     * @param video_params  the parameters to use for the output video
     */
    void initVideo(const Demuxer &demuxer, AVCodecID codec_id, AVPixelFormat pix_fmt,
                   const VideoParameters &video_params);

    /**
     * Initialize the audio processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream of packets
     * @param codec_id      the ID of the codec to use for the output audio
     */
    void initAudio(const Demuxer &demuxer, AVCodecID codec_id);

    /**
     * Initialize the output file.
     * WARNING: This function must be called after initializing all the desired processing chains
     * with initVideo() and initAudio()
     */
    void initOutput();

    /**
     * Send the packet to the processing chain corresponding to its type.
     * If 'async' was set to true when building the Pipeline,
     * the background threads will handle the packet processing and this function will
     * return immediately, otherwise the processing will be handled in
     * a synchronous way and this function will return only once it's completed
     * @param packet        the packet to send to che processing chain (if NULL, an exception will be thrown)
     * @param packet_type   the type of the packet to process
     */
    void feed(av::PacketUPtr packet, av::MediaType packet_type);

    /**
     * Flush the processing pipelines and close the output file.
     * If 'async' was set to true when building the Pipeline,
     * the background threads will be completely stopped before the actual flushing.
     */
    void flush();

    /**
     * Print the informations about the internal demuxer, decoders and encoders
     */
    void printInfo() const;
};