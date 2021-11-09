#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#include <math.h>
#include <string.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#define __STDC_CONSTANT_MACROS

// FFMPEG LIBRARIES
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"

// libav resample

#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/file.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/audio_fifo.h"

// lib swresample

#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

class ScreenRecorder {
private:
    /* Information about the input format (container) */
    AVFormatContext *in_fmt_ctx_;
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

    /* Counter for the audio frames pulled from the FIFO buffer */
    int audio_frames_counter_;

    int video_framerate_;
    int audio_samplerate_;

    int in_video_stream_idx_;
    int in_audio_stream_idx_;

    int offset_x_;
    int offset_y_;
    int width_;
    int height_;

    const char *output_file_;

    int SetVideoOptions();

    int InitVideoConverter();
    int InitAudioConverter();

    int InitVideoEncoder();
    int InitAudioEncoder();

    int ConvertEncodeStoreVideoPkt(AVPacket *in_packet);
    int ConvertEncodeStoreAudioPkt(AVPacket *in_packet);

public:
    ScreenRecorder();
    ~ScreenRecorder();

    int OpenInputDevices();
    int InitOutputFile();
    int CaptureFrames();
    int SelectArea();
};

#endif
