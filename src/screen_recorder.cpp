#include <screen_recorder.h>

#include <cstdlib>
#include <sstream>
#include <stdexcept>

#ifdef LINUX
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <winreg.h>

#include <string>
#endif

#include "duration_logger.h"

#define DURATION_LOGGING 0
#define FRAMERATE_LOGGING 0

ScreenRecorder::ScreenRecorder() {
    out_video_pix_fmt_ = AV_PIX_FMT_YUV420P;
    out_video_codec_id_ = AV_CODEC_ID_H264;
    out_audio_codec_id_ = AV_CODEC_ID_AAC;

#ifdef LINUX
    in_fmt_name_ = "x11grab";
    in_audio_fmt_name_ = "alsa";
#elif _WIN32
    in_fmt_name_ = "dshow";
#else
    in_fmt_name_ = "avfoundation";
#endif

    video_encoder_options_.insert({"preset", "ultrafast"});

    avdevice_register_all();
}

ScreenRecorder::~ScreenRecorder() {
    if (recorder_thread_.joinable()) recorder_thread_.join();
}

void ScreenRecorder::setDisplayResolution() const {
#ifdef WINDOWS
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
        throw std::runtime_error("Error opening key");
    }
#endif
}

void ScreenRecorder::initInput() {
    std::map<std::string, std::string> demuxer_options;

#ifdef _WIN32
    setDisplayResolution();
    demuxer_options.insert({"rtbufsize", "1024M"});
#else

    {
        std::stringstream framerate_ss;
        framerate_ss << video_framerate_;
        demuxer_options.insert({"framerate", framerate_ss.str()});
    }

#ifdef LINUX

    {
        std::stringstream video_size_ss;
        video_size_ss << video_width_ << "x" << video_height_;
        demuxer_options.insert({"video_size", video_size_ss.str()});
    }
    demuxer_options.insert({"show_region", "1"});
    /*
    device_name_ss << getenv("DISPLAY") << ".0+" << video_offset_x_ << "," << video_offset_y_;
    audio_device_name_ss << "hw:0,0";
    */
    /* set the offsets to 0 since they won't be used for cropping */
    video_offset_x_ = video_offset_y_ = 0;

#else  // macOS

    demuxer_options.insert({"pixel_format", "uyvy422"});
    demuxer_options.insert({"capture_cursor", "0"});
    /*
    device_name_ss << "1:";
    if (capture_audio_) device_name_ss << "0";
    */

#endif

#endif

    demuxer_ = std::make_unique<Demuxer>(in_fmt_name_, device_name_, demuxer_options);
    demuxer_->openInput();
    video_decoder_ = std::make_unique<Decoder>(demuxer_->getVideoParams());

    if (capture_audio_) {
#ifdef LINUX
        audio_demuxer_ =
            std::make_unique<Demuxer>(in_audio_fmt_name_, audio_device_name_, std::map<std::string, std::string>());
        audio_demuxer_->openInput();
        auto params = audio_demuxer_->getAudioParams();
#else
        auto params = demuxer_->getAudioParams();
#endif
        audio_decoder_ = std::make_unique<Decoder>(params);
    }
}

void ScreenRecorder::initOutput() {
    muxer_ = std::make_unique<Muxer>(output_file_);

    video_encoder_ =
        std::make_unique<VideoEncoder>(out_video_codec_id_, video_encoder_options_, muxer_->getGlobalHeaderFlags(),
                                       video_width_, video_height_, out_video_pix_fmt_, video_framerate_);
    muxer_->addVideoStream(video_encoder_->getCodecContext());

    if (capture_audio_) {
#ifdef LINUX
        auto params = audio_demuxer_->getAudioParams();
#else
        auto params = demuxer_->getAudioParams();
#endif
        audio_encoder_ =
            std::make_unique<AudioEncoder>(out_audio_codec_id_, audio_encoder_options_, muxer_->getGlobalHeaderFlags(),
                                           params->channels, params->sample_rate);
        muxer_->addAudioStream(audio_encoder_->getCodecContext());
    }

    muxer_->openFile();
}

void ScreenRecorder::initConverters() {
    video_converter_ = std::make_unique<VideoConverter>(
        video_decoder_->getCodecContext(), video_encoder_->getCodecContext(), video_offset_x_, video_offset_y_);

    if (capture_audio_) {
        audio_converter_ =
            std::make_unique<AudioConverter>(audio_decoder_->getCodecContext(), audio_encoder_->getCodecContext());
    }
}

void ScreenRecorder::printInfo() const {
    std::cout << "########## Streams Info ##########" << std::endl;
    demuxer_->dumpInfo();
#ifdef LINUX
    if (capture_audio_) audio_demuxer_->dumpInfo(1);
#endif
    muxer_->dumpInfo();
    std::cout << "Video framerate: " << video_framerate_ << " fps";
    if (video_framerate_ > 30)
        std::cout << " (WARNING: you may experience video frame loss and audio dropouts with high fps)";
    std::cout << std::endl;
}

void ScreenRecorder::setParams(const std::string &video_device, const std::string &audio_device,
                               const std::string &output_file, int video_width, int video_height, int video_offset_x,
                               int video_offset_y, int framerate) {
    if (framerate <= 0) throw std::runtime_error("Video framerate must be a positive number");
    if (video_width < 0 || video_height < 0) throw std::runtime_error("video width and height must be >= 0");
    if (video_offset_x < 0 || video_offset_y < 0) throw std::runtime_error("video offsets must be >= 0");
    if (video_device.empty()) throw std::runtime_error("video device not specified");
    if (output_file.empty()) throw std::runtime_error("output file not specified");

    if (video_width % 2) {
        std::cerr << "WARNING: the specified width is not an even number (it will be increased by 1)" << std::endl;
        video_width++;
    }

    if (video_height % 2) {
        std::cerr << "WARNING: the specified height is not an even number (it will be increased by 1)" << std::endl;
        video_height++;
    }

    /* capture_audio_ has to be set BEFORE device_name_ */
    capture_audio_ = !audio_device.empty();

    {
        std::stringstream device_name_ss;
#if defined(_WIN32)
        if (capture_audio_) device_name_ss << "audio=" << audio_device << ":";
        device_name_ss << "video=" << video_device;
#elif defined(LINUX)
        device_name_ss << video_device;  // getenv("DISPLAY") << ".0+" << video_offset_x_ << "," << video_offset_y_
        if (capture_audio) audio_device_name_ = audio_device;  // hw:0,0
#else  // macOS
        device_name_ss << video_device << ":";               // 1
        if (capture_audio_) device_name_ss << audio_device;  // 0
#endif
        device_name_ = device_name_ss.str();
    }

    output_file_ = output_file;
    video_framerate_ = framerate;
    video_width_ = video_width;
    video_height_ = video_height;
    video_offset_x_ = video_offset_x;
    video_offset_y_ = video_offset_y;
}

void ScreenRecorder::checkVideoSize() {
    auto params = demuxer_->getVideoParams();

    if (!video_width_) video_width_ = params->width;
    if (!video_height_) video_height_ = params->height;

    if ((video_offset_x_ + video_width_) > params->width)
        throw std::runtime_error("maximum width exceeds the display's one");
    if ((video_offset_y_ + video_height_) > params->height)
        throw std::runtime_error("maximum height exceeds the display's one");
}

void ScreenRecorder::start(const std::string &video_device, const std::string &audio_device,
                           const std::string &output_file, int video_width, int video_height, int video_offset_x,
                           int video_offset_y, int framerate) {
    try {
        setParams(video_device, audio_device, output_file, video_width, video_height, video_offset_x, video_offset_y,
                  framerate);
        initInput();
        checkVideoSize();
        initOutput();
        initConverters();
    } catch (const std::exception &e) {
        std::string details(e.what());
        throw std::runtime_error("Error during initialization of video-recorder's internal structures (" + details +
                                 ")");
    }

    std::cout << std::endl;
    printInfo();
    std::cout << std::endl;

    stop_capture_ = false;
    paused_ = false;

    recorder_thread_ = std::thread([this]() {
        std::cout << "Recording..." << std::endl;
        try {
            capture();
        } catch (const std::exception &e) {
            std::cerr << "Fatal error during capturing (" << e.what() << "), terminating..." << std::endl;
            exit(1);
        }
    });
}

void ScreenRecorder::stop() {
    {
        std::unique_lock<std::mutex> ul{mutex_};
        stop_capture_ = true;
        cv_.notify_all();
    }

    if (recorder_thread_.joinable()) recorder_thread_.join();

    muxer_->closeFile();
}

void ScreenRecorder::pause() {
    std::unique_lock<std::mutex> ul{mutex_};
    if (paused_ || stop_capture_) return;
    paused_ = true;
    std::cout << "Recording paused" << std::endl;
    cv_.notify_all();
}

void ScreenRecorder::resume() {
    std::unique_lock<std::mutex> ul{mutex_};
    if (!paused_ || stop_capture_) return;
    paused_ = false;
    std::cout << "Recording resumed" << std::endl;
    cv_.notify_all();
}

void ScreenRecorder::estimateFramerate() {
    auto estimated_framerate = 1000000 * video_frame_counter_ / (av_gettime() - start_time_);
#if FRAMERATE_LOGGING
    std::cout << "Estimated framerate: " << estimated_framerate << " fps" << std::endl;
#else
    if (estimated_framerate < (video_framerate_ - 1)) dropped_frame_counter_++;
    if (dropped_frame_counter_ == 2) {
        std::cerr << "WARNING: it looks like you're dropping some frames (estimated " << estimated_framerate
                  << " fps), try to lower the fps" << std::endl;
        dropped_frame_counter_ = 0;
    }
#endif
}

void ScreenRecorder::processConvertedFrame(const AVFrame *frame, av::DataType data_type) {
    const Encoder *encoder;

    if (data_type == av::DataType::video) {
        encoder = video_encoder_.get();
        if (video_frame_counter_ % video_framerate_ == 0) estimateFramerate();
    } else if (capture_audio_ && (data_type == av::DataType::audio)) {
        encoder = audio_encoder_.get();
    } else {
        throw std::runtime_error("Frame of unknown type received");
    }

    bool encoder_received = false;
    while (!encoder_received) {
        encoder_received = encoder->sendFrame(frame);

        while (true) {
            auto packet = encoder->getPacket();
            if (!packet) break;
            std::unique_lock<std::mutex> ul{mutex_};
            muxer_->writePacket(std::move(packet), data_type);
        }
    }
}

void ScreenRecorder::processPacket(const AVPacket *packet, av::DataType data_type) {
#if DURATION_LOGGING
    DurationLogger dl("Audio packet processed in ");
#endif
    const Decoder *decoder;
    const Converter *converter;

    if (data_type == av::DataType::video) {
        decoder = video_decoder_.get();
        converter = video_converter_.get();
    } else if (capture_audio_ && (data_type == av::DataType::audio)) {
        decoder = audio_decoder_.get();
        converter = audio_converter_.get();
    } else {
        throw std::runtime_error("Packet of unknown type received");
    }

    int64_t &frame_counter = (data_type == av::DataType::audio) ? audio_frame_counter_ : video_frame_counter_;

    bool decoder_received = false;
    while (!decoder_received) {
        decoder_received = decoder->sendPacket(packet);

        while (true) {
            auto frame = decoder->getFrame();
            if (!frame) break;

            bool converter_received = false;
            while (!converter_received) {
                converter_received = converter->sendFrame(frame.get());

                while (true) {
                    auto converted_frame = converter->getFrame(frame_counter);
                    if (!converted_frame) break;

                    frame_counter++;

                    processConvertedFrame(converted_frame.get(), data_type);
                }
            }
        }
    }
}

void ScreenRecorder::flushPipelines() {
    /* flush video pipeline */
    processPacket(nullptr, av::DataType::video);
    processConvertedFrame(nullptr, av::DataType::video);

    /* flush audio pipeline */
    if (capture_audio_) {
        processPacket(nullptr, av::DataType::audio);
        processConvertedFrame(nullptr, av::DataType::audio);
    }

    /* flush output queue */
    muxer_->writePacket(nullptr, av::DataType::none);
}

void ScreenRecorder::captureFrames(Demuxer *demuxer, bool handle_start_time) {
    while (true) {
        {
            std::unique_lock<std::mutex> ul{mutex_};
            bool handle_pause = paused_;
            int64_t pause_start_time;

            if (handle_pause) {
                if (handle_start_time) pause_start_time = av_gettime();
#ifndef MACOS
                demuxer->closeInput();
#endif
            }

            cv_.wait(ul, [this]() { return (!paused_ || stop_capture_); });
            if (stop_capture_) break;

            if (handle_pause) {
#ifndef MACOS
                demuxer->openInput();
#endif
                if (handle_start_time) start_time_ += (av_gettime() - pause_start_time);
            }
        }

        auto [packet, packet_type] = demuxer->readPacket();
        if (packet) processPacket(packet.get(), packet_type);
    }
}

void ScreenRecorder::capture() {
    /* start counting for PTS */
    video_frame_counter_ = 0;
    audio_frame_counter_ = 0;

    /* start counting for fps estimation */
    dropped_frame_counter_ = -1;  // wait for an extra second at the beginning to allow the framerate to stabilize
    start_time_ = av_gettime();

#ifdef LINUX
    std::thread audio_capturer;
    std::exception_ptr e_ptr;

    if (capture_audio_) {
        audio_capturer = std::thread([this, &e_ptr]() {
            try {
                captureFrames(audio_demuxer_.get());
            } catch (...) {
                e_ptr = std::current_exception();
                std::unique_lock<std::mutex> ul{mutex_};
                stop_capture_ = true;
            }
        });
    }
#endif

    captureFrames(demuxer_.get(), true);

#ifdef LINUX
    if (audio_capturer.joinable()) audio_capturer.join();
    if (e_ptr) std::rethrow_exception(e_ptr);
#endif

    flushPipelines();
}

void ScreenRecorder::listAvailableDevices() {
    std::string dummy_device_name;
    std::map<std::string, std::string> options;
    options.insert({"list_devices", "true"});

#ifdef WINDOWS
    dummy_device_name = "dummy";
#endif

    auto demuxer = std::make_unique<Demuxer>(in_fmt_name_, dummy_device_name, options);
    try {
        std::cout << "Available devices:" << std::endl;
        demuxer->openInput();
    } catch (...) {
        std::cout << std::endl;
    }
}