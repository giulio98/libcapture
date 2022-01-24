#include "capturer.h"

#ifdef WINDOWS
#include <windows.h>
#include <winreg.h>
#endif

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "format/demuxer.h"
#include "process/pipeline.h"
#include "utils/log_level_setter.h"
#include "utils/thread_guard.h"

#define THROW_TEST_EXCEPTION 0  // TO-DO: remove

static void makeAvVerbose(const bool verbose) {
    if (verbose) {
        av_log_set_level(AV_LOG_VERBOSE);
        // av_log_set_level(AV_LOG_DEBUG);
    } else {
        av_log_set_level(AV_LOG_ERROR);
    }
}

static std::string getInputFormatName(bool audio = false) {
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

static std::string generateInputDeviceName(const std::string &video_device, const std::string &audio_device,
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

static std::map<std::string, std::string> generateDemuxerOptions(const VideoParameters &video_params) {
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

Capturer::Capturer(const bool verbose) : verbose_(verbose) {
    makeAvVerbose(verbose_);
    avdevice_register_all();
}

Capturer::~Capturer() {
    stopCapture();
    if (capturer_.joinable()) capturer_.join();
}

bool Capturer::stopCapture() {
    {
        std::lock_guard lg(m_);
        if (stopped_) return false;
        stopped_ = true;
        cv_.notify_all();
    }
    return true;
}

std::future<void> Capturer::start(const std::string &video_device, const std::string &audio_device,
                                  const std::string &output_file, VideoParameters video_params) {
    if (!stopped_) throw std::runtime_error("Recording already in progress");

    if (video_device.empty()) throw std::runtime_error("Video device not specified");
    if (output_file.empty()) throw std::runtime_error("Output file not specified");

    bool capture_audio = !audio_device.empty();

    AVPixelFormat video_pix_fmt = AV_PIX_FMT_YUV420P;
    AVCodecID video_codec_id = AV_CODEC_ID_H264;
    AVCodecID audio_codec_id = AV_CODEC_ID_AAC;

    Demuxer demuxer;
    std::optional<Demuxer> audio_demuxer;  // Linux only

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
        async = capture_audio;
#endif
        /* init Pipeline */
        pipeline_ = std::make_unique<Pipeline>(output_file, async);
    }

    pipeline_->initVideo(demuxer, video_codec_id, video_pix_fmt, video_params);

    /* init audio structures, if necessary */
    if (capture_audio) {
#ifdef LINUX
        /* init audio demuxer and pipeline */
        const std::string audio_device_name = generateInputDeviceName("", audio_device, video_params);
        audio_demuxer = Demuxer(getInputFormatName(true), audio_device_name, std::map<std::string, std::string>());
        audio_demuxer->openInput();
        pipeline_->initAudio(*audio_demuxer, audio_codec_id);
#else
        pipeline_->initAudio(demuxer, audio_codec_id);
#endif
    }

    pipeline_->initOutput();

    /* Print info about structures (if verbose) */
    if (verbose_) {
        std::cout << std::endl;
        demuxer.printInfo();
#ifdef LINUX
        if (capture_audio) audio_demuxer.value().printInfo(1);
#endif
        pipeline_->printInfo();
        std::cout << std::endl;
    }

    std::promise<void> p;
    auto f = p.get_future();

    {
        /*
         * To avoid having the status variable "stopped_" set to false even if the capturer thread fails to start,
         * update it only once the capturer has been completely and successfully initialized.
         * To avoid an immediate return from capture(), a lock on the mutex m_ must be acquired
         */

        std::lock_guard lg(m_);

        /* in case of exception when starting the thread, capturer_ will be left untouched ("empty") */
        capturer_ = std::thread(
            [this, demuxer = std::move(demuxer), audio_demuxer = std::move(audio_demuxer), p = std::move(p)]() mutable {
                try {
                    if (audio_demuxer) {
                        capture(demuxer, *audio_demuxer);
                    } else {
                        capture(demuxer);
                    }
                    p.set_value();
                } catch (...) {
                    p.set_exception(std::current_exception());
                }
            });
        /* note that if audio_demuxer was not empty, it still contains a (now "empty") demuxer here */

        paused_ = false;
        stopped_ = false;  // set stopped_ to false only when everything is properly set-up

    }  // release the mutex, now the capturer[s] will enter in the main loop inside capture()

    return f;
}

void Capturer::stop() {
    if (!stopCapture()) return;
    if (capturer_.joinable()) capturer_.join();
    pipeline_->flush();
    pipeline_.reset();
}

void Capturer::pause() {
    std::lock_guard lg(m_);
    if (paused_ || stopped_) return;
    paused_ = true;
    cv_.notify_all();
}

void Capturer::resume() {
    std::lock_guard lg(m_);
    if (!paused_ || stopped_) return;
    paused_ = false;
    cv_.notify_all();
}

void Capturer::capture(Demuxer &video_demuxer, Demuxer &audio_demuxer) {
    std::thread audio_capturer;
    std::exception_ptr e_ptr;

    {
        ThreadGuard tg(audio_capturer);

        audio_capturer = std::thread(
            [this, &e_ptr](Demuxer &audio_demuxer) {
                try {
                    capture(audio_demuxer);
                } catch (...) {
                    stopCapture();
                    e_ptr = std::current_exception();
                }
            },
            std::ref(audio_demuxer));

        try {
            capture(video_demuxer);
        } catch (...) {
            stopCapture();
            throw;
        }
    }  // join audio_capturer in any case

    if (e_ptr) std::rethrow_exception(e_ptr);
}

void Capturer::capture(Demuxer &demuxer) {
    bool after_pause;
    int64_t last_pts = 0;
    int64_t pts_offset = 0;
    bool adjust_pts_offset = false;
    std::chrono::milliseconds sleep_interval(1);

#if THROW_TEST_EXCEPTION
    int counter = 0;
#endif

    while (true) {
        {
            std::unique_lock ul(m_);
            after_pause = paused_;
#ifndef MACOS
            if (paused_) demuxer.closeInput();
#endif
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

#if THROW_TEST_EXCEPTION
        if (++counter == 300) throw std::runtime_error("UGLY ERROR");
#endif
    }
}

void Capturer::setVerbose(const bool verbose) {
    verbose_ = verbose;
    makeAvVerbose(verbose_);
}

void Capturer::listAvailableDevices() const {
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