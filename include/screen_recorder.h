#pragma once

#include <iostream>
#include <thread>
#include <mutex>

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
    /* Information about the input format (container) */
    AVFormatContext *in_fmt_ctx_;
#ifdef __linux__
    /* Information about the input format (container) */
    AVFormatContext *in_audio_fmt_ctx_;
#endif
    /* Information about the output format (container) */
    AVFormatContext *out_fmt_ctx_;

    /* Context for the decode/encode operations (input) */
    AVCodecContext *in_video_codec_ctx_;
    /* Context for the decode/encode operations (input) */
    AVCodecContext *in_audio_codec_ctx_;
    /* Context for the decode/encode operations (output) */
    AVCodecContext *out_video_codec_ctx_;
    /* Context for the decode/encode operations (output) */
    AVCodecContext *out_audio_codec_ctx_;

    /* Component used to encode/decode the streams (input) */
    AVCodec *in_video_codec_;
    /* Component used to encode/decode the streams (input) */
    AVCodec *in_audio_codec_;
    /* Component used to encode/decode the streams (output) */
    AVCodec *out_video_codec_;
    /* Component used to encode/decode the streams (output) */
    AVCodec *out_audio_codec_;

    /* Additional options for the muxer */
    AVDictionary *video_options_;

    /* Output video stream */
    AVStream *out_video_stream_;
    /* Output audio stream */
    AVStream *out_audio_stream_;

    SwsContext *video_converter_ctx_;
    SwrContext *audio_converter_ctx_;
    AVAudioFifo *audio_fifo_buf_;

    /* Counter of video frames used to compute PTS */
    int video_frame_counter_;
    /* Counter of audio frames used to compute PTS */
    int audio_frame_counter_;

    AVPixelFormat video_pix_fmt_;
    int video_framerate_;

    int in_video_stream_idx_;
    int in_audio_stream_idx_;

    int offset_x_;
    int offset_y_;
    int width_;
    int height_;

    const char *output_file_;

    int SetVideoOptions();

    int OpenInputDevice(AVFormatContext *&in_fmt_ctx, AVInputFormat *in_fmt, const char *device_name,
                        AVDictionary **options);

    int InitVideoEncoder();
    int InitAudioEncoder();

    int InitVideoConverter();
    int InitAudioConverter();

    /* Convert, encode and write in the output file the video packet */
    int ProcessVideoPkt(AVPacket *in_packet);
    /* Convert, encode and write in the output file the audio packet */
    int ProcessAudioPkt(AVPacket *in_packet);
    /* Convert the audio frame and write it to the audio FIFO buffer */
    int WriteAudioFrameToFifo(AVFrame *in_frame);

    int OpenInputDevices();
    int InitOutputFile();
    int CaptureFrames();
    int SelectArea();
    std::mutex m;

    std::thread tvideo;
    bool stopCapture;

public:
    ScreenRecorder();
    ~ScreenRecorder();

    
    void start();
    void stop();
    //TODO
    // void pause()
    // void resume()
};