#pragma once

/* FFmpeg Libraries */
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

#include <array>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "deleter.h"

namespace av {

/**
 * Data types (Audio or Video).
 * WARNING: NumTypes is NOT a valid data type and it's only used to properly size data structures
 * in order to use Video and Audio as indices
 * (DO NOT change the order)
 */
enum MediaType { None = -1, Video, Audio, NumTypes };

/**
 * Array of media types valid as indices
 */
constexpr std::array<MediaType, MediaType::NumTypes> validMediaTypes = {MediaType::Video, MediaType::Audio};

/**
 * Whether the given type is valid
 * @param type the data type to check
 * @return whether the data type is a valid one
 */
constexpr bool validMediaType(MediaType type) { return (type > MediaType::None && type < MediaType::NumTypes); }

using PacketUPtr = std::unique_ptr<AVPacket, DeleterPP<av_packet_free>>;
using FrameUPtr = std::unique_ptr<AVFrame, DeleterPP<av_frame_free>>;
using InFormatContextUPtr = std::unique_ptr<AVFormatContext, DeleterPP<avformat_close_input>>;
using FormatContextUPtr = std::unique_ptr<AVFormatContext, DeleterP<avformat_free_context>>;
using CodecContextUPtr = std::unique_ptr<AVCodecContext, DeleterPP<avcodec_free_context>>;
using FilterGraphUPtr = std::unique_ptr<AVFilterGraph, DeleterPP<avfilter_graph_free>>;
using FilterInOutUPtr = std::unique_ptr<AVFilterInOut, DeleterPP<avfilter_inout_free>>;
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

inline std::map<std::string, std::string> dict2map(const AVDictionary *dict) {
    std::map<std::string, std::string> map;
    const AVDictionaryEntry *entry = nullptr;
    do {
        entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX);
        if (entry) map.insert({entry->key, entry->value});
    } while (entry);
    return map;
}

}  // namespace av