#pragma once

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "audio_encoder.h"
#include "decoder.h"
#include "demuxer.h"
#include "ffmpeg_libs.h"
#include "muxer.h"
#include "video_converter.h"
#include "video_encoder.h"

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
    std::unique_ptr<Decoder> video_dec_;
    std::unique_ptr<Decoder> audio_dec_;
    std::shared_ptr<VideoEncoder> video_enc_;
    std::shared_ptr<AudioEncoder> audio_enc_;
    std::shared_ptr<VideoConverter> video_conv_;

    std::map<std::string, std::string> video_enc_options_;
    std::map<std::string, std::string> audio_enc_options_;

    /* Thread responsible for recording video and audio */
    std::thread recorder_thread_;

    /* Audio converter context */
    SwrContext *audio_converter_ctx_;

    /* FIFO buffer for the audio used for resampling */
    AVAudioFifo *audio_fifo_buf_;

    /* Counter of video frames used to compute PTSs */
    int video_frame_counter_;
    /* Counter of audio frames used to compute PTSs */
    int audio_frame_counter_;

    int InitAudioConverter();

    /* Convert the audio frame and write it to the audio FIFO buffer */
    int WriteAudioFrameToFifo(AVFrame *frame);

    int ProcessVideoPkt(AVPacket *packet);

    int ProcessAudioPkt(AVPacket *packet);

    int EncodeWriteFrame(const AVFrame *frame, int audio_video);

    int FlushEncoders();

    int OpenInputDevices();
    int InitOutputFile();
    int CaptureFrames();
    int SelectArea();

public:
    ScreenRecorder();
    ~ScreenRecorder();

    void Start(const std::string &output_file, bool audio);
    void Stop();
    void Pause();
    void Resume();
};