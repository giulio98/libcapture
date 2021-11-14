#pragma once

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "audio_converter.h"
#include "audio_encoder.h"
#include "decoder.h"
#include "demuxer.h"
#include "ffmpeg_libs.h"
#include "muxer.h"
#include "video_converter.h"
#include "video_encoder.h"

enum AVType { audio, video };

class ScreenRecorder {
    bool record_audio_;

    int offset_x_;
    int offset_y_;
    int width_;
    int height_;

    std::string output_file_;

    std::mutex mutex_;
    std::condition_variable cv_;

    bool stop_capture_;
    bool paused_;

    int video_framerate_;
    AVPixelFormat out_video_pix_fmt_;
    AVCodecID out_video_codec_id_;
    AVCodecID out_audio_codec_id_;

    std::string in_fmt_name_;
    std::string in_device_name_;

    std::unique_ptr<Demuxer> demuxer_;
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
    int video_frame_counter_;
    /* Counter of audio frames used to compute PTSs */
    int audio_frame_counter_;

    void processVideoPacket(const AVPacket *packet);

    void processAudioPacket(const AVPacket *packet);

    void encodeWriteFrame(const AVFrame *frame, AVType audio_video);

    void flushPipelines();

    void captureFrames();

    int selectArea();

public:
    ScreenRecorder();
    ~ScreenRecorder();

    void Start(const std::string &output_file, bool audio);
    void Stop();
    void Pause();
    void Resume();
};