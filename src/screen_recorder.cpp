#include "screen_recorder.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifdef WINDOWS
#include <windows.h>
#include <winreg.h>
#endif

#include "log_callback_setter.h"
#include "log_level_setter.h"

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
#if defined(_WIN32)
    if (!audio_device.empty()) device_name_ss << "audio=" << audio_device << ":";
    device_name_ss << "video=" << video_device;
#elif defined(LINUX)
    if (!video_device.empty()) {
        device_name_ss << video_device;
        if (video_params.offset_x || video_params.offset_y) {
            device_name_ss << "+" << video_params.offset_x << "," << video_params.offset_y;
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
#ifdef _WIN32
    setDisplayResolution(video_params.framerate);
    demuxer_options.insert({"rtbufsize", "1024M"});
#else
    {
        std::stringstream framerate_ss;
        framerate_ss << video_params.framerate;
        demuxer_options.insert({"framerate", framerate_ss.str()});
    }
#ifdef LINUX
    if (video_params.width && video_params.height) {
        std::stringstream video_size_ss;
        video_size_ss << video_params.width << "x" << video_params.height;
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

static void checkParams(const std::string &video_device, const std::string &output_file,
                        const VideoParameters &video_params) {
    if (video_params.framerate <= 0) throw std::runtime_error("Video framerate must be a positive number");
    if (video_params.width < 0 || video_params.height < 0)
        throw std::runtime_error("video width and height must be >= 0");
    if (video_params.offset_x < 0 || video_params.offset_y < 0) throw std::runtime_error("video offsets must be >= 0");
    if (video_params.width % 2) throw std::runtime_error("the specified width is not an even number");
    if (video_params.height % 2) throw std::runtime_error("the specified height is not an even number");
    if (video_device.empty()) throw std::runtime_error("video device not specified");
    if (output_file.empty()) throw std::runtime_error("output file not specified");
}

ScreenRecorder::ScreenRecorder(bool verbose) : stopped_(true), verbose_(verbose) {
    makeAvVerbose(verbose_);
    avdevice_register_all();
}

ScreenRecorder::~ScreenRecorder() {
    if (recorder_thread_.joinable()) recorder_thread_.join();
#ifdef LINUX
    if (audio_recorder_thread_.joinable()) audio_recorder_thread_.join();
#endif
}

void ScreenRecorder::start(const std::string &video_device, const std::string &audio_device,
                           const std::string &output_file, VideoParameters video_params) {
    checkParams(video_device, output_file, video_params);

    std::unique_lock ul{m_};
    if (!stopped_) throw std::runtime_error("Recording already in progress");

    AVPixelFormat video_pix_fmt = AV_PIX_FMT_YUV420P;
    AVCodecID video_codec_id = AV_CODEC_ID_H264;
    AVCodecID audio_codec_id = AV_CODEC_ID_AAC;

    std::unique_ptr<Demuxer> demuxer;
#ifdef LINUX
    std::unique_ptr<Demuxer> audio_demuxer;
#endif

    {
        /* init Muxer */
        muxer_ = std::make_shared<Muxer>(output_file);

        /* init Demuxer */
        const std::string device_name = generateInputDeviceName(video_device, audio_device, video_params);
        const std::map<std::string, std::string> demuxer_options = generateDemuxerOptions(video_params);
        demuxer = std::make_unique<Demuxer>(getInputFormatName(), device_name, demuxer_options);
        demuxer->openInput();

        bool use_processors;
#ifdef LINUX
        video_params.offset_x = video_params.offset_y = 0;  // No cropping is performed on Linux
        use_processors = false;
#else
        use_processors = true;
#endif
        /* init Pipeline */
        pipeline_ = std::make_unique<Pipeline>(muxer_, use_processors);
        pipeline_->initVideo(demuxer.get(), video_codec_id, video_params, video_pix_fmt);

        /* init audio structures, if necessary */
        if (!audio_device.empty()) {
#ifdef LINUX
            /* init audio demuxer and pipeline */
            const std::string audio_device_name = generateInputDeviceName("", audio_device, video_params);
            audio_demuxer = std::make_unique<Demuxer>(getInputFormatName(true), audio_device_name,
                                                      std::map<std::string, std::string>());
            audio_demuxer->openInput();
            pipeline_->initAudio(audio_demuxer.get(), audio_codec_id);
#else
            pipeline_->initAudio(demuxer.get(), audio_codec_id);
#endif
        }
    }

    muxer_->openFile();

    if (verbose_) {
        std::cout << std::endl;
        demuxer->printInfo();
#ifdef LINUX
        if (audio_demuxer) audio_demuxer->printInfo(1);
#endif
        muxer_->printInfo();
        pipeline_->printInfo();
        std::cout << std::endl;
    }

    stop_capture_ = false;
    paused_ = false;

    auto recorder_fn = [this](std::unique_ptr<Demuxer> demuxer) {
        try {
            capture(std::move(demuxer));
        } catch (const std::exception &e) {
            std::cerr << "Fatal error during capturing (" << e.what() << "), terminating..." << std::endl;
            exit(1);
        }
    };

    recorder_thread_ = std::thread(recorder_fn, std::move(demuxer));
#ifdef LINUX
    if (audio_demuxer) audio_recorder_thread_ = std::thread(recorder_fn, std::move(audio_demuxer));
#endif

    stopped_ = false;
}

void ScreenRecorder::stop() {
    {
        std::unique_lock ul{m_};
        stop_capture_ = true;
        cv_.notify_all();
    }

    if (recorder_thread_.joinable()) recorder_thread_.join();
#ifdef LINUX
    if (audio_recorder_thread_.joinable()) audio_recorder_thread_.join();
#endif

    {
        std::unique_lock ul{m_};
        pipeline_->flush();
        muxer_->closeFile();
        pipeline_.reset();
        muxer_.reset();
        stopped_ = true;
    }
}

void ScreenRecorder::pause() {
    std::unique_lock<std::mutex> ul{m_};
    if (paused_ || stop_capture_) return;
    paused_ = true;
    cv_.notify_all();
}

void ScreenRecorder::resume() {
    std::unique_lock<std::mutex> ul{m_};
    if (!paused_ || stop_capture_) return;
    paused_ = false;
    cv_.notify_all();
}

void ScreenRecorder::capture(std::unique_ptr<Demuxer> demuxer) {
    if (!demuxer) throw std::runtime_error("received demuxer is NULL");

    int64_t last_pts = 0;
    int64_t pts_offset = 0;
    bool adjust_pts_offset = false;
    std::chrono::milliseconds sleep_interval(1);

    while (true) {
        bool after_pause;
        {
            std::unique_lock<std::mutex> ul{m_};
#ifndef MACOS
            if (paused_) demuxer->closeInput();
#endif
            after_pause = paused_;
            cv_.wait(ul, [this]() { return (!paused_ || stop_capture_); });
            if (stop_capture_) break;
        }

        if (after_pause) {
            adjust_pts_offset = true;
#ifdef MACOS
            demuxer->flush();
#else
            demuxer->openInput();
#endif
        }

        auto [packet, packet_type] = demuxer->readPacket();
        if (!packet) {
            std::this_thread::sleep_for(sleep_interval);
            continue;
        }
        if (!av::isDataTypeValid(packet_type)) throw std::runtime_error("Invalid packet type received from demuxer");

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
        // LogCallbackSetter lcs(log_callback);
        LogLevelSetter lls(AV_LOG_INFO);
        try {
            demuxer.openInput();
        } catch (...) {
        }
    }
    std::cout << std::endl;
}