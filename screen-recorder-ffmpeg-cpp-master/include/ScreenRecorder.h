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
    /* Input container */
    AVFormatContext *inFormatContext;
    /* Output container */
    AVFormatContext *outFormatContext;

    /* Context for the decode/encode operations (input) */
    AVCodecContext *inCodecContext;
    /* Context for the decode/encode operations (output) */
    AVCodecContext *outCodecContext;

    /* Information about the audio format (container) */
    AVFormatContext *audioFormatContext;

    /* Context for the decode/encode operations (input) */
    AVCodecContext *audioCodecContext;

    /* Component used to encode/decode the streams (input) */
    AVCodec *inCodec;
    /* Component used to encode/decode the streams (output) */
    AVCodec *outCodec;

    AVCodec *audioCodec;

    /* Additional options for the muxer */
    AVDictionary *options;

    /* Output video stream */
    AVStream *outVideoStream;

    const char *outputFile;

    int codecId;
    int videoStreamIdx;
    int audioStreamIdx;

    int offsetX;
    int offsetY;
    int width;
    int height;

public:
    ScreenRecorder();
    ~ScreenRecorder();

    /* function to initiate communication with display library */
    int OpenCamera();
    int OpenMic();
    int InitOutputFile();
    int SelectArea();
    int CaptureVideoFrames();
};

#endif
