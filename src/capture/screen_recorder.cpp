#include "screen_recorder.h"

#ifdef WINDOWS
#include <windows.h>
#include <winreg.h>
#endif

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "format/demuxer.h"
#include "format/muxer.h"
#include "process/pipeline.h"
#include "utils/log_level_setter.h"

static void makeAvVerbose(bool verbose) {
    if (verbose) {
        av_log_set_level(AV_LOG_VERBOSE);
        // av_log_set_level(AV_LOG_DEBUG);
    } else {
        av_log_set_level(AV_LOG_ERROR);
    }
}

static const std::string getInputFormatName(bool audio = false) {
#if defined(LINUX)
    if (audio) {
        return "alsa";
    } else {
        return "x11grab";
    }
#elif defined(WINDOWS)
    return "dshow";
#else  // macOS
    return "avfoundation";
#endif
}

static const std::string generateInputDeviceName(const std::string &video_device, const std::string &audio_device,
                                                 const VideoParameters &video_params) {
    std::stringstream device_name_ss;
#if defined(WINDOWS)
    if (!audio_device.empty()) device_name_ss << "audio=" << audio_device << ":";
    device_name_ss << "video=" << video_device;
#elif defined(LINUX)
    if (!video_device.empty()) {
        device_name_ss << video_device;
        auto [offset_x, offset_y] = video_params.getVideoOffset();
        if (offset_x || offset_y) {
            device_name_ss << "+" << offset_x << "," << offset_y;
        }
    } else {
        device_name_ss << audio_device;
    }
#else  // macOS
    device_name_ss << video_device << ":" << audio_device;
#endif
    return device_name_ss.str();
}

#ifdef WINDOWS
static void setDisplayResolution(int framerate) {
    int x1, y1, x2, y2, resolution_width, resolution_height;
    x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
    x2 = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    y2 = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    resolution_width = x2 - x1;
    resolution_height = y2 - y1;
    HKEY hkey;
    DWORD dwDisposition;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\screen-capture-recorder"), 0, nullptr, 0, KEY_WRITE, nullptr,
                       &hkey, &dwDisposition) == ERROR_SUCCESS) {
        DWORD dwType, dwSize;
        dwType = REG_DWORD;
        dwSize = sizeof(DWORD);
        DWORD rofl = framerate;
        RegSetValueEx(hkey, TEXT("default_max_fps"), 0, dwType, (PBYTE)&rofl, dwSize);
        rofl = resolution_width;
        RegSetValueEx(hkey, TEXT("capture_width"), 0, dwType, (PBYTE)&rofl, dwSize);
        rofl = resolution_height;
        RegSetValueEx(hkey, TEXT("capture_height"), 0, dwType, (PBYTE)&rofl, dwSize);
        rofl = 0;
        RegSetValueEx(hkey, TEXT("start_x"), 0, dwType, (PBYTE)&rofl, dwSize);
        rofl = 0;
        RegSetValueEx(hkey, TEXT("start_y"), 0, dwType, (PBYTE)&rofl, dwSize);
        RegCloseKey(hkey);
    } else {
        throw std::runtime_error("Error opening key when setting display resolution");
    }
}
#endif

static const std::map<std::string, std::string> generateDemuxerOptions(const VideoParameters &video_params) {
    std::map<std::string, std::string> demuxer_options;
#ifdef WINDOWS
    setDisplayResolution(video_params.getFramerate());
    demuxer_options.insert({"rtbufsize", "1024M"});
#else
    {
        std::stringstream framerate_ss;
        framerate_ss << video_params.getFramerate();
        demuxer_options.insert({"framerate", framerate_ss.str()});
    }
#ifdef LINUX
    auto [width, height] = video_params.getVideoSize();
    if (width && height) {
        std::stringstream video_size_ss;
        video_size_ss << width << "x" << height;
        demuxer_options.insert({"video_size", video_size_ss.str()});
    }
    demuxer_options.insert({"show_region", "0"});
#else  // macOS
    demuxer_options.insert({"pixel_format", "uyvy422"});
    demuxer_options.insert({"capture_cursor", "0"});
#endif
#endif
    return demuxer_options;
}

ScreenRecorder::ScreenRecorder(bool verbose) : verbose_(verbose) {
    makeAvVerbose(verbose_);
    avdevice_register_all();
}

ScreenRecorder::~ScreenRecorder() { stopCapturers(); }

void ScreenRecorder::stopCapturers() {
    {
        std::unique_lock ul{m_};
        stopped_ = true;
        cv_.notify_all();
    }
    if (capturer_.joinable()) capturer_.join();
#ifdef LINUX
    if (audio_capturer_.joinable()) audio_capturer_.join();
#endif
}

void ScreenRecorder::start(const std::string &video_device, const std::string &audio_device,
                           const std::string &output_file, VideoParameters video_params) {
    if (!stopped_) throw std::runtime_error("Recording already in progress");

    if (video_device.empty()) throw std::runtime_error("Video device not specified");
    if (output_file.empty()) throw std::runtime_error("Output file not specified");

    bool capture_audio = !audio_device.empty();

    AVPixelFormat video_pix_fmt = AV_PIX_FMT_YUV420P;
    AVCodecID video_codec_id = AV_CODEC_ID_H264;
    AVCodecID audio_codec_id = AV_CODEC_ID_AAC;

    Demuxer demuxer;
#ifdef LINUX
    Demuxer audio_demuxer;
#endif

    /* init Muxer */
    muxer_ = std::make_shared<Muxer>(output_file);

    { /* init Demuxer */
        const std::string device_name = generateInputDeviceName(video_device, audio_device, video_params);
        const std::map<std::string, std::string> demuxer_options = generateDemuxerOptions(video_params);
        demuxer = Demuxer(getInputFormatName(), device_name, demuxer_options);
        demuxer.openInput();
    }

    { /* init Pipeline */
        bool async;
#ifdef LINUX
        video_params.setVideoOffset(0, 0);  // No cropping is performed on Linux
        async = false;
#else
        async = true;
#endif
        /* init Pipeline */
        pipeline_ = std::make_unique<Pipeline>(muxer_, async);
    }

    pipeline_->initVideo(demuxer, video_codec_id, video_pix_fmt, video_params);

    /* init audio structures, if necessary */
    if (capture_audio) {
#ifdef LINUX
        /* init audio demuxer and pipeline */
        const std::string audio_device_name = generateInputDeviceName("", audio_device, video_params);
        audio_demuxer = Demuxer(getInputFormatName(true), audio_device_name, std::map<std::string, std::string>());
        audio_demuxer.openInput();
        pipeline_->initAudio(audio_demuxer, audio_codec_id);
#else
        pipeline_->initAudio(demuxer, audio_codec_id);
#endif
    }

    /* Open output file */
    muxer_->openFile();

    /* Print info about structures (if verbose) */
    if (verbose_) {
        std::cout << std::endl;
        demuxer.printInfo();
#ifdef LINUX
        if (capture_audio) audio_demuxer.printInfo(1);
#endif
        muxer_->printInfo();
        pipeline_->printInfo();
        std::cout << std::endl;
    }

    stopped_ = false;
    paused_ = false;

    auto capturer_fn = [this](Demuxer demuxer) {
        try {
            capture(std::move(demuxer));
        } catch (const std::exception &e) {
            std::cerr << "Fatal error during capturing (" << e.what() << "), terminating..." << std::endl;
            exit(1);
        }
    };

    capturer_ = std::thread(capturer_fn, std::move(demuxer));
#ifdef LINUX
    if (capture_audio) audio_capturer_ = std::thread(capturer_fn, std::move(audio_demuxer));
#endif
}

void ScreenRecorder::stop() {
    stopCapturers();
    pipeline_->flush();
    muxer_->closeFile();
    pipeline_.reset();
    muxer_.reset();
}

void ScreenRecorder::pause() {
    std::unique_lock<std::mutex> ul{m_};
    if (paused_ || stopped_) return;
    paused_ = true;
    cv_.notify_all();
}

void ScreenRecorder::resume() {
    std::unique_lock<std::mutex> ul{m_};
    if (!paused_ || stopped_) return;
    paused_ = false;
    cv_.notify_all();
}

void ScreenRecorder::capture(Demuxer demuxer) {
    int64_t last_pts = 0;
    int64_t pts_offset = 0;
    bool adjust_pts_offset = false;
    std::chrono::milliseconds sleep_interval(1);

    while (true) {
        bool after_pause;
        {
            std::unique_lock<std::mutex> ul{m_};
#ifndef MACOS
            if (paused_) demuxer.closeInput();
#endif
            after_pause = paused_;
            cv_.wait(ul, [this]() { return (!paused_ || stopped_); });
            if (stopped_) break;
        }

        if (after_pause) {
            adjust_pts_offset = true;
#ifdef MACOS
            demuxer.flush();
#else
            demuxer.openInput();
#endif
        }

        auto [packet, packet_type] = demuxer.readPacket();
        if (!packet) {
            std::this_thread::sleep_for(sleep_interval);
            continue;
        }
        if (!av::validMediaType(packet_type)) throw std::runtime_error("Invalid packet type received from demuxer");

        if (adjust_pts_offset) pts_offset += (packet->pts - last_pts);
        last_pts = packet->pts;
        if (adjust_pts_offset) {
            adjust_pts_offset = false;
        } else {
            packet->pts -= pts_offset;
            pipeline_->feed(std::move(packet), packet_type);
        }
    }
}

void ScreenRecorder::setVerbose(bool verbose) {
    verbose_ = verbose;
    makeAvVerbose(verbose_);
}

void ScreenRecorder::listAvailableDevices() const {
    std::string dummy_device_name;
    std::map<std::string, std::string> options;
    options.insert({"list_devices", "true"});

#ifdef WINDOWS
    dummy_device_name = "dummy";
#endif

    Demuxer demuxer(getInputFormatName(), dummy_device_name, options);
    std::cout << "##### Available Devices #####" << std::endl;
    {
        LogLevelSetter lls(AV_LOG_INFO);
        try {
            demuxer.openInput();
        } catch (...) {
        }
    }
    std::cout << std::endl;
}