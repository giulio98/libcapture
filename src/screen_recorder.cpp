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
        av_log_set_level(AV_LOG_PRINT_LEVEL);
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

    /* init Muxer */
    muxer_ = std::make_shared<Muxer>(output_file);

    /* init Demuxer */
    const std::string device_name = generateInputDeviceName(video_device, audio_device, video_params);
    const std::map<std::string, std::string> demuxer_options = generateDemuxerOptions(video_params);
    auto demuxer = std::make_shared<Demuxer>(getInputFormatName(), device_name, demuxer_options);
    demuxer->openInput();

#ifdef LINUX
    video_params.offset_x = video_params.offset_y = 0;  // No cropping is performed on Linux
#endif

    /* init Pipeline */
    auto pipeline = std::make_unique<Pipeline>(demuxer, muxer_);
    pipeline->initVideo(video_codec_id, video_params, video_pix_fmt);

    /* init audio structures, if necessary */
    if (!audio_device.empty()) {
#ifdef LINUX
        /* init audio demuxer and pipeline */
        const std::string audio_device_name = generateInputDeviceName("", audio_device, video_params);
        auto audio_demuxer = std::make_shared<Demuxer>(getInputFormatName(true), audio_device_name,
                                                       std::map<std::string, std::string>());
        audio_demuxer->openInput();
        auto audio_pipeline = std::make_unique<Pipeline>(audio_demuxer, muxer_);
        audio_pipeline->initAudio(audio_codec_id);
#else
        pipeline->initAudio(audio_codec_id);
#endif
    }

    muxer_->openFile();

    if (verbose_) {
        std::cout << std::endl;
        demuxer->printInfo();
#ifdef LINUX
        audio_demuxer->printInfo();
#endif
        muxer_->printInfo();
        pipeline->printInfo();
#ifdef LINUX
        audio_pipeline->printInfo();
#endif
        std::cout << std::endl;
    }

    stop_capture_ = false;
    paused_ = false;

    auto recorder_fn = [this](std::shared_ptr<Demuxer> demuxer, std::unique_ptr<Pipeline> pipeline) {
        try {
            capture(std::move(demuxer), std::move(pipeline));
        } catch (const std::exception &e) {
            std::cerr << "Fatal error during capturing (" << e.what() << "), terminating..." << std::endl;
            exit(1);
        }
    };

    recorder_thread_ = std::thread(recorder_fn, std::move(demuxer), std::move(pipeline));
#ifdef LINUX
    audio_recorder_thread_ = std::thread(recorder_fn, std::move(audio_demuxer), std::move(audio_pipeline));
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

    muxer_->closeFile();
    muxer_.reset();

    {
        std::unique_lock ul{m_};
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

void ScreenRecorder::capture(std::shared_ptr<Demuxer> demuxer, std::unique_ptr<Pipeline> pipeline) {
    if (!demuxer) throw std::runtime_error("received demuxer is NULL");
    if (!pipeline) throw std::runtime_error("received pipeline is NULL");

    bool recovering_from_pause = false;
    std::chrono::milliseconds sleep_interval(1);

    while (true) {
        {
            std::unique_lock<std::mutex> ul{m_};
            bool handle_pause = paused_;

            if (handle_pause) {
#ifndef MACOS
                demuxer->closeInput();
#endif
            }

            cv_.wait(ul, [this]() { return (!paused_ || stop_capture_); });
            if (stop_capture_) break;

            if (handle_pause) {
                recovering_from_pause = true;
#ifndef MACOS
                demuxer->openInput();
#else
                demuxer->flush();
#endif
            }
        }

        if (pipeline->step(recovering_from_pause)) {
            if (recovering_from_pause) recovering_from_pause = false;
        } else {
            std::this_thread::sleep_for(sleep_interval);
        }
    }

    pipeline->flush();
}

void ScreenRecorder::setVerbose(bool verbose) {
    verbose_ = verbose;
    makeAvVerbose(verbose_);
}

void ScreenRecorder::listAvailableDevices() {
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