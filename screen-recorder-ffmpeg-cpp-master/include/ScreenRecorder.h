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
// #include "libavfilter/avfiltergraph.h"
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

// lib swresample

#include "libswscale/swscale.h"
}

class ScreenRecorder {
private:
    /* Input video container */
    AVFormatContext *inVideoFormatContext;
    /* Input audio container */
    AVFormatContext *inAudioFormatContext;
    /* Output container */
    AVFormatContext *outFormatContext;

    /* Context for the input decode/encode operations (video) */
    AVCodecContext *inVideoCodecContext;
    /* Context for the input decode/encode operations (audio) */
    AVCodecContext *inAudioCodecContext;
    /* Context for the output decode/encode operations (video) */
    AVCodecContext *outVideoCodecContext;
    /* Context for the input decode/encode operations (audio) */
    AVCodecContext *outAudioCodecContext;

    /* Component used to encode/decode the streams (input) */
    AVCodec *inVideoCodec;
    /* Component used to encode/decode the streams (input) */
    AVCodec *inAudioCodec;
    /* Component used to encode/decode the streams (output) */
    AVCodec *outVideoCodec;
    /* Component used to encode/decode the streams (output) */
    AVCodec *outAudioCodec;

    /* Additional options for the muxer */
    AVDictionary *videoOptions;
    AVDictionary *audioOptions;

    /* Output video stream */
    AVStream *outVideoStream;
    /* Output audio stream */
    AVStream *outAudioStream;

    const char *outputFile;

    int codecId;
    int videoStreamIdx;
    int audioStreamIdx;

    int offsetX;
    int offsetY;
    int width;
    int height;

    int PrepareVideoEncoder();
    int PrepareAudioEncoder();

public:
    ScreenRecorder();
    ~ScreenRecorder();

    /* function to initiate communication with display library */
    int OpenCamera();
    int OpenMic();
    int InitOutputFile();
    int SelectArea();
    int CaptureVideoFrames();
    int CaptureAudioFrames();
};

#endif
