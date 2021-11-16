#pragma once

#include <memory>

#include "deleter.h"

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

namespace av {
enum class DataType { none, audio, video };

using PacketPtr = std::unique_ptr<AVPacket, DeleterPP<av_packet_free>>;
using FramePtr = std::unique_ptr<AVFrame, DeleterPP<av_frame_free>>;
using InFormatContextPtr = std::unique_ptr<AVFormatContext, DeleterPP<avformat_close_input>>;
using FormatContextPtr = std::unique_ptr<AVFormatContext, DeleterP<avformat_free_context>>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, DeleterPP<avcodec_free_context>>;
using SwsContextPtr = std::unique_ptr<SwsContext, DeleterP<sws_freeContext>>;
using SwrContextPtr = std::unique_ptr<SwrContext, DeleterPP<swr_free>>;
using AudioFifoPtr = std::unique_ptr<AVAudioFifo, DeleterP<av_audio_fifo_free>>;
using DictionaryPtr = std::unique_ptr<AVDictionary, DeleterPP<av_dict_free>>;
}  // namespace av