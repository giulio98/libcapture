#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "audio_converter.h"
#include "audio_encoder.h"
#include "common.h"
#include "decoder.h"
#include "demuxer.h"
#include "muxer.h"
#include "video_converter.h"
#include "video_encoder.h"

class ScreenRecorder {
    bool capture_audio_;

    int video_offset_x_;
    int video_offset_y_;
    int video_width_;
    int video_height_;
    int video_framerate_;
    AVPixelFormat out_video_pix_fmt_;
    AVCodecID out_video_codec_id_;
    AVCodecID out_audio_codec_id_;
    std::string output_file_;

    bool stop_capture_;
    bool paused_;
    std::mutex mutex_;
    std::condition_variable cv_;

    std::string in_fmt_name_;
    std::unique_ptr<Demuxer> demuxer_;
#ifdef LINUX
    std::string in_audio_fmt_name_;
    std::unique_ptr<Demuxer> audio_demuxer_;
#endif
    std::unique_ptr<Muxer> muxer_;
    std::unique_ptr<Decoder> video_decoder_;
    std::unique_ptr<Decoder> audio_decoder_;
    std::shared_ptr<VideoEncoder> video_encoder_;
    std::shared_ptr<AudioEncoder> audio_encoder_;
    std::unique_ptr<VideoConverter> video_converter_;
    std::unique_ptr<AudioConverter> audio_converter_;

    std::map<std::string, std::string> video_encoder_options_;
    std::map<std::string, std::string> audio_encoder_options_;

    /* Thread responsible for recording video and audio */
    std::thread recorder_thread_;

    /* Counter of video frames used to compute PTSs */
    int64_t video_frame_counter_;
    /* Counter of audio frames used to compute PTSs */
    int64_t audio_frame_counter_;

    /* Recording start time */
    int64_t start_time_;
    /* Counter of times in which the estimated framerate is lower than the specified one */
    int dropped_frame_counter_;

    void processVideoPacket(std::shared_ptr<const AVPacket> packet);

    void processAudioPacket(std::shared_ptr<const AVPacket> packet);

    void processConvertedFrame(std::shared_ptr<const AVFrame> frame, av::DataType audio_video);

    void flushPipelines();

    void captureFrames();

    int selectArea();

    void initInput();

    void initOutput();

    void initConverters();

    void printInfo();

    void estimateFramerate();

public:
    ScreenRecorder();

    ~ScreenRecorder();

    void start(const std::string &output_file, int framerate, bool capture_audio);

    void stop();

    void pause();

    void resume();
};