#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include "audio_converter.h"
#include "audio_encoder.h"
#include "common.h"
#include "decoder.h"
#include "demuxer.h"
#include "muxer.h"
#include "video_converter.h"
#include "video_encoder.h"
#include <vector>

class ScreenRecorder {
    bool capture_audio_{};

    int video_offset_x_{};
    int video_offset_y_{};
    int video_width_{};
    int video_height_{};
    int video_framerate_{};
    std::string device_name_;
#ifdef LINUX
    std::string audio_device_name_;
#endif
    AVPixelFormat out_video_pix_fmt_;
    AVCodecID out_video_codec_id_;
    AVCodecID out_audio_codec_id_;
    std::string output_file_;

    bool stop_capture_{};
    bool paused_{};
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
    std::unique_ptr<VideoEncoder> video_encoder_;
    std::unique_ptr<AudioEncoder> audio_encoder_;
    std::unique_ptr<VideoConverter> video_converter_;
    std::unique_ptr<AudioConverter> audio_converter_;

    std::map<std::string, std::string> video_encoder_options_;
    std::map<std::string, std::string> audio_encoder_options_;

    /* Thread responsible for recording video and audio */
    std::thread recorder_thread_;

    /* Counter of video frames used to compute PTSs */
    int64_t video_frame_counter_{};
    /* Counter of audio frames used to compute PTSs */
    int64_t audio_frame_counter_{};

    /* Recording start time */
    int64_t start_time_{};
    /* Counter of times in which the estimated framerate is lower than the specified one */
    int dropped_frame_counter_{};

    void setParams(const std::string &video_device, const std::string &audio_device, const std::string &output_file,
                   int video_width, int video_height, int video_offset_x, int video_offset_y, int framerate);

    void checkVideoSize();

    void processPacket(const AVPacket *packet, av::DataType data_type);

    void processConvertedFrame(const AVFrame *frame, av::DataType data_type);

    void flushPipelines();

    void captureFrames(Demuxer *demuxer, bool handle_start_time = false);

    void capture();

    void initInput();

    void initOutput();

    void initConverters();

    void printInfo() const;

    void estimateFramerate();
#ifdef WINDOWS
    void setDisplayResolution() const;
#endif
public:
    ScreenRecorder();

    ~ScreenRecorder();

    void start(const std::string &video_device, const std::string &audio_device, const std::string &output_file,
               int video_width, int video_height, int video_offset_x, int video_offset_y, int framerate);

    void stop();

    void pause();

    void resume();

    void listAvailableDevices();
#ifdef WINDOWS
    static std::vector<std::string> getInputAudioDevices() ;
#endif
};