#pragma once

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

/* FFMPEG LIBRARIES */
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/file.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

class ScreenRecorder {
    /* Video framerate (set at input-device opening time) */
    int video_framerate_;

    int offset_x_;
    int offset_y_;
    int width_;
    int height_;

    std::string output_file_;

    std::mutex mutex_;
    std::condition_variable cv_;

    bool stop_capture_;
    bool paused_;

    /* Thread responsible for recording video and audio */
    std::thread recorder_thread_;

    /* Input audio-video (video-only for Linux) format context (container) */
    AVFormatContext *in_fmt_ctx_;
#ifdef __linux__
    /* Linux-only input audio format context (container) */
    AVFormatContext *in_audio_fmt_ctx_;
#endif
    /* Output audio-video format context (container) */
    AVFormatContext *out_fmt_ctx_;

    /* Additional options for the video input format */
    AVDictionary *video_options_;

    /* Pixel format to which convert the video frames */
    AVPixelFormat out_video_pix_fmt_;

    /* Codec to which convert the video */
    AVCodecID out_video_codec_id_;

    /* Video decoder */
    AVCodec *in_video_codec_;
    /* Audio decoder */
    AVCodec *in_audio_codec_;

    /* Video encoder */
    AVCodec *out_video_codec_;
    /* Audio encoder */
    AVCodec *out_audio_codec_;

    /* Video decoder context */
    AVCodecContext *in_video_codec_ctx_;
    /* Audio decoder context */
    AVCodecContext *in_audio_codec_ctx_;

    /* Video encoder context */
    AVCodecContext *out_video_codec_ctx_;
    /* Audio encoder context */
    AVCodecContext *out_audio_codec_ctx_;

    /* Input video stream */
    AVStream *in_video_stream_;
    /* Input audio stream */
    AVStream *in_audio_stream_;

    /* Output video stream */
    AVStream *out_video_stream_;
    /* Output audio stream */
    AVStream *out_audio_stream_;

    /* Video converter context */
    SwsContext *video_converter_ctx_;
    /* Audio converter context */
    SwrContext *audio_converter_ctx_;

    /* FIFO buffer for the audio used for resampling */
    AVAudioFifo *audio_fifo_buf_;

    /* Counter of video frames used to compute PTSs */
    int video_frame_counter_;
    /* Counter of audio frames used to compute PTSs */
    int audio_frame_counter_;

    /* Set video_options_ */
    int SetVideoOptions();

    /**
     * Open an input device
     * @param in_fmt_ctx Input format to initialize. If NULL, it will be allocated
     * @param in_fmt Input format
     * @param device_name Name of the device to open
     * @param options Additional options to use when opening the input. May be NULL
     */
    int OpenInputDevice(AVFormatContext *&in_fmt_ctx, AVInputFormat *in_fmt, const char *device_name,
                        AVDictionary **options);

    /**
     * Initialize the audio/video decoder and its context
     * @param audio_video Whether the decoder is video (0) or audio (any other value)
     */
    int InitDecoder(int audio_video);

    /* Initialize the video encoder and its context */
    int InitVideoEncoder();
    /* Initialize the audio encoder and its context */
    int InitAudioEncoder();

    int InitVideoConverter();
    int InitAudioConverter();

    /* Convert the audio frame and write it to the audio FIFO buffer */
    int WriteAudioFrameToFifo(AVFrame *frame);

    /**
     * Encode a frame and write to out_fmt_ctx_
     * @param audio_video Whether the frame is video (0) or audio (any other value)
     */
    int EncodeWriteFrame(AVFrame *frame, int audio_video);

    /* Convert, encode and write to the output file the video packet */
    int ProcessVideoPkt(AVPacket *packet);

    /* Convert, encode and write to the output file the audio packet */
    int ProcessAudioPkt(AVPacket *packet);

    int FlushEncoders();

    int OpenInputDevices();
    int InitOutputFile();
    int CaptureFrames();
    int SelectArea();

public:
    ScreenRecorder();
    ~ScreenRecorder();

    void Start(const std::string &output_file);
    void Stop();
    void Pause();
    void Resume();
};