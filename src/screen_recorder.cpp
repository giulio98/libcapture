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

static std::string getInputDeviceName(const std::string &video_device, const std::string &audio_device,
                                      const VideoDimensions &video_dims) {
    std::stringstream device_name_ss;
#if defined(_WIN32)
    if (!audio_device.empty()) device_name_ss << "audio=" << audio_device << ":";
    device_name_ss << "video=" << video_device;
#elif defined(LINUX)
    if (!video_device.empty()) {
        device_name_ss << video_device;
        if (video_dims.offset_x || video_dims.offset_y) {
            device_name_ss << "+" << video_dims.offset_x << "," << video_dims.offset_y;
        }
    } else {
        device_name_ss << audio_device;
    }
#else  // macOS
    device_name_ss << video_device << ":" << audio_device;
#endif
    return device_name_ss.str();
}

static std::map<std::string, std::string> getDemuxerOptions(const VideoDimensions &video_dims, int framerate) {
    std::map<std::string, std::string> demuxer_options;

#ifdef _WIN32
    setDisplayResolution();
    demuxer_options.insert({"rtbufsize", "1024M"});
#else
    {
        std::stringstream framerate_ss;
        framerate_ss << framerate;
        demuxer_options.insert({"framerate", framerate_ss.str()});
    }
#ifdef LINUX
    if (video_dims.width && video_dims.height) {
        std::stringstream video_size_ss;
        video_size_ss << video_dims.width << "x" << video_dims.height;
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
                        const VideoDimensions &video_dims, int framerate) {
    if (framerate <= 0) throw std::runtime_error("Video framerate must be a positive number");
    if (video_dims.width < 0 || video_dims.height < 0) throw std::runtime_error("video width and height must be >= 0");
    if (video_dims.offset_x < 0 || video_dims.offset_y < 0) throw std::runtime_error("video offsets must be >= 0");
    if (video_dims.width % 2) throw std::runtime_error("the specified width is not an even number");
    if (video_dims.height % 2) throw std::runtime_error("the specified height is not an even number");
    if (video_device.empty()) throw std::runtime_error("video device not specified");
    if (output_file.empty()) throw std::runtime_error("output file not specified");
}

ScreenRecorder::ScreenRecorder() {
    out_video_pix_fmt_ = AV_PIX_FMT_YUV420P;
    out_video_codec_id_ = AV_CODEC_ID_H264;
    out_audio_codec_id_ = AV_CODEC_ID_AAC;

#if defined(LINUX)
    in_fmt_name_ = "x11grab";
    in_audio_fmt_name_ = "alsa";
#elif defined(WINDOWS)
    in_fmt_name_ = "dshow";
#else  // macOS
    in_fmt_name_ = "avfoundation";
#endif

    avdevice_register_all();
}

ScreenRecorder::~ScreenRecorder() {
    if (recorder_thread_.joinable()) recorder_thread_.join();
#ifdef LINUX
    if (audio_recorder_thread_.joinable()) audio_recorder_thread_.join();
#endif
}

#ifdef WINDOWS
void ScreenRecorder::setDisplayResolution() const {
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
        DWORD rofl = video_framerate_;
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

void ScreenRecorder::start(const std::string &video_device, const std::string &audio_device,
                           const std::string &output_file, VideoDimensions video_dims, int framerate, bool verbose) {
    checkParams(video_device, output_file, video_dims, framerate);

    verbose_ = verbose;
    if (verbose_) {
        av_log_set_level(AV_LOG_VERBOSE);
        // av_log_set_level(AV_LOG_DEBUG);
    } else {
        av_log_set_level(AV_LOG_PRINT_LEVEL);
    }

    /* init Muxer */
    muxer_ = std::make_unique<Muxer>(output_file);
    /* init Demuxer */
    std::string device_name = getInputDeviceName(video_device, audio_device, video_dims);
    std::map<std::string, std::string> demuxer_options = getDemuxerOptions(video_dims, framerate);
    demuxer_ = std::make_unique<Demuxer>(in_fmt_name_, device_name, demuxer_options);
    demuxer_->openInput();
    /* init Pipeline */
    pipeline_ = std::make_unique<Pipeline>(demuxer_, muxer_);
#ifdef LINUX
    video_dims.offset_x = video_dims.offset_y = 0;  // No cropping is performed on Linux
#endif
    pipeline_->initVideo(out_video_codec_id_, video_dims, out_video_pix_fmt_);
    /* init audio structures, if necessary */
    if (!audio_device.empty()) {
#ifdef LINUX
        /* init audio demuxer and pipeline */
        std::string audio_device_name = getInputDeviceName("", audio_device, video_dims);
        audio_demuxer_ =
            std::make_shared<Demuxer>(in_audio_fmt_name_, audio_device_name, std::map<std::string, std::string>());
        audio_demuxer_->openInput();
        audio_pipeline_ = std::make_unique<Pipeline>(audio_demuxer_, muxer_);
        audio_pipeline_->initAudio(out_audio_codec_id_);
#else
        pipeline_->initAudio(out_audio_codec_id_);
#endif
    }
    muxer_->openFile();

    stop_capture_ = false;
    paused_ = false;

    startRecorder(recorder_thread_, demuxer_.get(), pipeline_.get());
#ifdef LINUX
    startRecorder(audio_recorder_thread_, audio_demuxer_.get(), audio_pipeline_.get());
#endif

    std::cout << "Recording..." << std::endl;
}

void ScreenRecorder::stop() {
    {
        std::unique_lock ul{m_};
        stop_capture_ = true;
        status_cv_.notify_all();
    }

    // std::cout << "Recording stopped, waiting for video processing to complete..." << std::flush;
    if (recorder_thread_.joinable()) recorder_thread_.join();
#ifdef LINUX
    if (audio_recorder_thread_.joinable()) audio_recorder_thread_.join();
#endif
    // std::cout << " done" << std::endl;

    muxer_->closeFile();
    muxer_.reset();
    demuxer_.reset();
    pipeline_.reset();
#ifdef LINUX
    audio_demuxer_.reset();
    audio_pipeline_.reset();
#endif
}

void ScreenRecorder::pause() {
    std::unique_lock<std::mutex> ul{m_};
    if (paused_ || stop_capture_) return;
    paused_ = true;
    std::cout << "Recording paused" << std::endl;
    status_cv_.notify_all();
}

void ScreenRecorder::resume() {
    std::unique_lock<std::mutex> ul{m_};
    if (!paused_ || stop_capture_) return;
    paused_ = false;
    std::cout << "Recording resumed..." << std::endl;
    status_cv_.notify_all();
}

void ScreenRecorder::capture(Demuxer *demuxer, Pipeline *pipeline) {
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

            status_cv_.wait(ul, [this]() { return (!paused_ || stop_capture_); });
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

void ScreenRecorder::startRecorder(std::thread &recorder, Demuxer *demuxer, Pipeline *pipeline) {
    if (recorder.joinable()) recorder.join();
    recorder = std::thread([this, demuxer, pipeline]() {
        try {
            capture(demuxer, pipeline);
        } catch (const std::exception &e) {
            std::cerr << "Fatal error during capturing (" << e.what() << "), terminating..." << std::endl;
            exit(1);
        }
    });
}

void ScreenRecorder::listAvailableDevices() {
    std::string dummy_device_name;
    std::map<std::string, std::string> options;
    options.insert({"list_devices", "true"});

#ifdef WINDOWS
    dummy_device_name = "dummy";
#endif

    Demuxer demuxer(in_fmt_name_, dummy_device_name, options);
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