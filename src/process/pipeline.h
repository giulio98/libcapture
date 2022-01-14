#pragma once

#include <array>
#include <condition_variable>
#include <thread>
#include <vector>

#include "common/common.h"
#include "converter.h"
#include "decoder.h"
#include "encoder.h"
#include "video_parameters.h"

class Demuxer;
class Muxer;

class Pipeline {
    std::array<bool, av::MediaType::NumTypes> managed_type_ = {false, false};
    std::shared_ptr<Muxer> muxer_;
    std::array<Decoder, av::MediaType::NumTypes> decoders_;
    std::array<Encoder, av::MediaType::NumTypes> encoders_;
    std::array<Converter, av::MediaType::NumTypes> converters_;

    bool use_background_processors_;
    std::mutex m_;
    bool stop_;
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
     * @param muxer                     the muxer to send the processed packets to (WARNING: the muxer must not be
     * opened until the Pipeline initialization is complete)
     * @param use_background_processors whether the pipeline should use background threads to handle the processing
     * (recommended when a single demuxer will provide both video and audio packets)
     */
    explicit Pipeline(std::shared_ptr<Muxer> muxer, bool use_background_processors = false);

    Pipeline(const Pipeline &) = delete;

    ~Pipeline();

    Pipeline &operator=(const Pipeline &) = delete;

    /**
     * Initialize the video processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream of packets
     * @param codec_id      the ID of the codec to use for the output video
     * @param video_params  the parameters to use for the output video
     * @param pix_fmt       the pixel format to use for the output video
     */
    void initVideo(const Demuxer &demuxer, AVCodecID codec_id, const VideoParameters &video_params,
                   AVPixelFormat pix_fmt);

    /**
     * Initialize the audio processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream of packets
     * @param codec_id      the ID of the codec to use for the output audio
     */
    void initAudio(const Demuxer &demuxer, AVCodecID codec_id);

    /**
     * Send the packet to the processing chain corresponding to its type.
     * If 'use_background_processors' was set to true when building the Pipeline,
     * the background threads will handle the packet processing and this function will
     * return immediately, otherwise the processing will be handled in
     * a synchronous way and this function will return only once it's completed
     * @param packet        the packet to send to che processing chain
     * @param packet_type   the type of the packet to process
     */
    void feed(av::PacketUPtr packet, av::MediaType packet_type);

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