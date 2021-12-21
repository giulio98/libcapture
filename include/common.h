#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

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
enum DataType { audio = 0, video, none };

using PacketUPtr = std::unique_ptr<AVPacket, DeleterPP<av_packet_free>>;
using FrameUPtr = std::unique_ptr<AVFrame, DeleterPP<av_frame_free>>;
using InFormatContextUPtr = std::unique_ptr<AVFormatContext, DeleterPP<avformat_close_input>>;
using FormatContextUPtr = std::unique_ptr<AVFormatContext, DeleterP<avformat_free_context>>;
using CodecContextUPtr = std::unique_ptr<AVCodecContext, DeleterPP<avcodec_free_context>>;
using SwrContextUPtr = std::unique_ptr<SwrContext, DeleterPP<swr_free>>;
using FilterGraphUPtr = std::unique_ptr<AVFilterGraph, DeleterPP<avfilter_graph_free>>;
using FilterInOutUPtr = std::unique_ptr<AVFilterInOut, DeleterPP<avfilter_inout_free>>;
using AudioFifoUPtr = std::unique_ptr<AVAudioFifo, DeleterP<av_audio_fifo_free>>;
using DictionaryUPtr = std::unique_ptr<AVDictionary, DeleterPP<av_dict_free>>;

inline DictionaryUPtr map2dict(const std::map<std::string, std::string> &map) {
    AVDictionary *dict = nullptr;
    for (const auto &[key, val] : map) {
        if (av_dict_set(&dict, key.c_str(), val.c_str(), 0) < 0) {
            if (dict) av_dict_free(&dict);
            throw std::runtime_error("Cannot set " + key + "in dictionary");
        }
    }
    return DictionaryUPtr(dict);
}

}  // namespace av