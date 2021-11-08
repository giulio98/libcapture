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
    /* Input format */
    AVInputFormat *pAVInputFormat;
    /* Output format */
    AVOutputFormat *output_format;

    /* Properties of the codec used by a stream */
    AVCodecParameters *pAVCodecParameters;

    /* Context for the decode/encode operations (input) */
    AVCodecContext *pAVCodecContext;
    /* Context for the decode/encode operations (output) */
    AVCodecContext *outAVCodecContext;

    /* Information about the input format (container) */
    AVFormatContext *pAVFormatContext;
    /* Information about the output format (container) */
    AVFormatContext *outAVFormatContext;

    /* Component used to encode/decode the streams (input) */
    AVCodec *pAVCodec;
    /* Component used to encode/decode the streams (output) */
    AVCodec *outAVCodec;

    /* Compressed (encoded) video data */
    AVPacket *pAVPacket;

    /* Decoded video data (input) */
    AVFrame *pAVFrame;
    /* Decoded video data (output) */
    AVFrame *outFrame;

    /* Additional options for the muxer */
    AVDictionary *options;

    AVOutputFormat *outAVOutputFormat;

    /* Output video stream */
    AVStream *video_st;

    AVFrame *outAVFrame;

    const char *dev_name;
    const char *output_file;

    double video_pts;

    int out_size;
    int codec_id;
    int value;
    int VideoStreamIndx;

    int offsetX;
    int offsetY;
    int width;
    int height;

public:
    ScreenRecorder();
    ~ScreenRecorder();

    /* function to initiate communication with display library */
    int openCamera();
    int init_outputfile();
#ifdef __linux__
    int selectArea();
#endif
    int CaptureVideoFrames();
};

#endif
