#include "../include/screen_recorder.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef LINUX
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

#include <sstream>

#include "../include/duration_logger.h"

#define DURATION_LOGGING 0
#define FRAMERATE_LOGGING 1

ScreenRecorder::ScreenRecorder() {
    out_video_pix_fmt_ = AV_PIX_FMT_YUV420P;
    out_video_codec_id_ = AV_CODEC_ID_H264;
    out_audio_codec_id_ = AV_CODEC_ID_AAC;
#ifdef LINUX
    in_fmt_name_ = "x11grab";
    in_audio_fmt_name_ = "alsa";
#else
    in_fmt_name_ = "avfoundation";
#endif
    video_encoder_options_.insert({"preset", "ultrafast"});
    avdevice_register_all();
}

ScreenRecorder::~ScreenRecorder() {
    if (recorder_thread_.joinable()) recorder_thread_.join();
}

void ScreenRecorder::initInput() {
    std::stringstream device_name;
    std::stringstream video_size;
    std::stringstream framerate;
    std::map<std::string, std::string> demux_options;

#ifdef LINUX
    device_name << getenv("DISPLAY") << ".0+" << video_offset_x_ << "," << video_offset_y_;
#else
    device_name << "1:";
    if (capture_audio_) device_name << "0";
#endif
    video_size << video_width_ << "x" << video_height_;
    framerate << video_framerate_;

    demux_options.insert({"video_size", video_size.str()});
    demux_options.insert({"framerate", framerate.str()});
#ifdef LINUX
    demux_options.insert({"show_region", "1"});
#else
    demux_options.insert({"capture_cursor", "0"});
#endif

    demuxer_ = std::make_unique<Demuxer>(in_fmt_name_, device_name.str(), demux_options);
    demuxer_->openInput();

    video_decoder_ = std::make_unique<Decoder>(demuxer_->getVideoParams());

    if (capture_audio_) {
#ifdef LINUX
        audio_demuxer_ = std::make_unique<Demuxer>(in_audio_fmt_name_, "hw:0,0", std::map<std::string, std::string>());
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
                                       demuxer_->getVideoParams(), out_video_pix_fmt_, video_framerate_);
    muxer_->addVideoStream(video_encoder_->getCodecContext());

    if (capture_audio_) {
#ifdef LINUX
        auto params = audio_demuxer_->getAudioParams();
#else
        auto params = demuxer_->getAudioParams();
#endif
        audio_encoder_ = std::make_unique<AudioEncoder>(out_audio_codec_id_, audio_encoder_options_,
                                                        muxer_->getGlobalHeaderFlags(), params);
        muxer_->addAudioStream(audio_encoder_->getCodecContext());
    }

    muxer_->openFile();
}

void ScreenRecorder::initConverters() {
    video_converter_ =
        std::make_unique<VideoConverter>(video_decoder_->getCodecContext(), video_encoder_->getCodecContext());

    if (capture_audio_) {
        audio_converter_ =
            std::make_unique<AudioConverter>(audio_decoder_->getCodecContext(), audio_encoder_->getCodecContext());
    }
}

void ScreenRecorder::printInfo() {
    std::cout << "########## Streams Info ##########" << std::endl;
    demuxer_->dumpInfo(0);
#ifdef LINUX
    if (capture_audio_) audio_demuxer_->dumpInfo(1);
#endif
    muxer_->dumpInfo();
    std::cout << "Video framerate: " << video_framerate_ << " fps";
    if (video_framerate_ > 30)
        std::cout << " (WARNING: you may experience video frame loss and audio dropouts with high fps)";
    std::cout << std::endl;
}

void ScreenRecorder::start(const std::string &output_file, int framerate, bool capture_audio) {
    output_file_ = output_file;
    video_framerate_ = framerate;
    capture_audio_ = capture_audio;
    stop_capture_ = false;
    paused_ = false;

    try {
        if (selectArea()) throw std::runtime_error("Failed to select area");
        initInput();
        initOutput();
        initConverters();
        printInfo();

        recorder_thread_ = std::thread([this]() {
            std::cout << "Recording..." << std::endl;
            try {
                capture();
            } catch (const std::exception &e) {
                std::cerr << "\nERROR: " << e.what() << ", terminating..." << std::endl;
                exit(1);
            }
        });

    } catch (const std::exception &e) {
        std::cerr << "\nERROR: " << e.what() << ", terminating..." << std::endl;
        exit(1);
    }
}

void ScreenRecorder::stop() {
    {
        std::unique_lock<std::mutex> ul{mutex_};
        stop_capture_ = true;
        paused_ = false;
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

void ScreenRecorder::processConvertedFrame(const AVFrame *frame, av::DataType frame_type) {
    const Encoder *encoder;

    if (frame_type == av::DataType::video) {
        encoder = video_encoder_.get();
        if (video_frame_counter_ % video_framerate_ == 0) estimateFramerate();
    } else if (frame_type == av::DataType::audio) {
        encoder = audio_encoder_.get();
    } else {
        throw std::runtime_error("frame type is unknown");
    }

    bool encoder_received = false;
    while (!encoder_received) {
        encoder_received = encoder->sendFrame(frame);

        while (true) {
            auto packet = encoder->getPacket();
            if (!packet) break;
            std::unique_lock<std::mutex> ul{mutex_};
            muxer_->writePacket(std::move(packet), frame_type);
        }
    }
}

void ScreenRecorder::processVideoPacket(const AVPacket *packet) {
#if DURATION_LOGGING
    DurationLogger dl("Video packet processed in ");
#endif

    bool decoder_received = false;
    while (!decoder_received) {
        decoder_received = video_decoder_->sendPacket(packet);

        while (true) {
            auto in_frame = video_decoder_->getFrame();
            if (!in_frame) break;

            auto out_frame = video_converter_->convertFrame(in_frame.get(), video_frame_counter_++);

            processConvertedFrame(out_frame.get(), av::DataType::video);
        }
    }
}

void ScreenRecorder::processAudioPacket(const AVPacket *packet) {
#if DURATION_LOGGING
    DurationLogger dl("Audio packet processed in ");
#endif

    bool decoder_received = false;
    while (!decoder_received) {
        decoder_received = audio_decoder_->sendPacket(packet);

        while (true) {
            auto in_frame = audio_decoder_->getFrame();
            if (!in_frame) break;

            bool converter_received = false;
            while (!converter_received) {
                converter_received = audio_converter_->sendFrame(in_frame.get());

                while (true) {
                    auto out_frame = audio_converter_->getFrame(audio_frame_counter_);
                    if (!out_frame) break;

                    audio_frame_counter_++;

                    processConvertedFrame(out_frame.get(), av::DataType::audio);
                }
            }
        }
    }
}

void ScreenRecorder::flushPipelines() {
    /* flush video decoder */
    processVideoPacket(nullptr);
    /* flush video encoder */
    processConvertedFrame(nullptr, av::DataType::video);

    if (capture_audio_) {
        /* flush audio decoder */
        processAudioPacket(nullptr);
        /* flush audio encoder */
        processConvertedFrame(nullptr, av::DataType::audio);
    }

    /* flush output queue */
    muxer_->writePacket(nullptr, av::DataType::none);
}

void ScreenRecorder::captureFrames(Demuxer *demuxer, bool handle_time) {
    while (true) {
        {
            std::unique_lock<std::mutex> ul{mutex_};
            bool handle_pause = paused_;
            int64_t pause_start_time;

            if (handle_pause) {
                if (handle_time) pause_start_time = av_gettime();
#ifndef MACOS
                demuxer->closeInput();
#endif
            }

            cv_.wait(ul, [this]() { return !paused_; });
            if (stop_capture_) break;

            if (handle_pause) {
#ifndef MACOS
                demuxer->openInput();
#endif
                if (handle_time) start_time_ += (av_gettime() - pause_start_time);
            }
        }

        auto [packet, packet_type] = demuxer->readPacket();
        if (!packet) continue;

        if (packet_type == av::DataType::video) {
            processVideoPacket(packet.get());
        } else if (capture_audio_ && (packet_type == av::DataType::audio)) {
            processAudioPacket(packet.get());
        } else {
            throw std::runtime_error("Unknown packet received from demuxer");
        }
    }
}

void ScreenRecorder::capture() {
#ifndef MACOS
    std::thread audio_capturer;
#endif

    /* start counting for PTS */
    video_frame_counter_ = 0;
    audio_frame_counter_ = 0;

    /* start counting for fps estimation */
    dropped_frame_counter_ = -1;  // wait for an extra second at the beginning to allow the framerate to stabilize
    start_time_ = av_gettime();

#ifndef MACOS
    if (capture_audio_) audio_capturer = std::thread([this]() { captureFrames(audio_demuxer_.get(), false); });
#endif

    captureFrames(demuxer_.get(), true);

#ifndef MACOS
    if (audio_capturer.joinable()) audio_capturer.join();
#endif

    flushPipelines();
}

int ScreenRecorder::selectArea() {
#ifdef LINUX
    XEvent ev;
    Display *disp = NULL;
    Screen *scr = NULL;
    Window root = 0;
    Cursor cursor, cursor2;
    XGCValues gcval;
    GC gc;
    int rx = 0, ry = 0, rw = 0, rh = 0;
    int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
    int btn_pressed = 0, done = 0;
    int threshold = 10;

    std::cout << "Select the area to record (click to select all the display)" << std::endl;

    disp = XOpenDisplay(NULL);
    if (!disp) return EXIT_FAILURE;

    scr = ScreenOfDisplay(disp, DefaultScreen(disp));

    root = RootWindow(disp, XScreenNumberOfScreen(scr));

    cursor = XCreateFontCursor(disp, XC_left_ptr);
    cursor2 = XCreateFontCursor(disp, XC_lr_angle);

    gcval.foreground = XWhitePixel(disp, 0);
    gcval.function = GXxor;
    gcval.background = XBlackPixel(disp, 0);
    gcval.plane_mask = gcval.background ^ gcval.foreground;
    gcval.subwindow_mode = IncludeInferiors;

    gc = XCreateGC(disp, root, GCFunction | GCForeground | GCBackground | GCSubwindowMode, &gcval);

    /* this XGrab* stuff makes XPending true ? */
    if ((XGrabPointer(disp, root, False, ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
                      GrabModeAsync, root, cursor, CurrentTime) != GrabSuccess))
        printf("couldn't grab pointer:");

    if ((XGrabKeyboard(disp, root, False, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess))
        printf("couldn't grab keyboard:");

    while (!done) {
        while (!done && XPending(disp)) {
            XNextEvent(disp, &ev);
            switch (ev.type) {
                case MotionNotify:
                    /* this case is purely for drawing rect on screen */
                    if (btn_pressed) {
                        if (rect_w) {
                            /* re-draw the last rect to clear it */
                            // XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
                        } else {
                            /* Change the cursor to show we're selecting a region */
                            XChangeActivePointerGrab(disp, ButtonMotionMask | ButtonReleaseMask, cursor2, CurrentTime);
                        }
                        rect_x = rx;
                        rect_y = ry;
                        rect_w = ev.xmotion.x - rect_x;
                        rect_h = ev.xmotion.y - rect_y;

                        if (rect_w < 0) {
                            rect_x += rect_w;
                            rect_w = 0 - rect_w;
                        }
                        if (rect_h < 0) {
                            rect_y += rect_h;
                            rect_h = 0 - rect_h;
                        }
                        /* draw rectangle */
                        // XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
                        XFlush(disp);
                    }
                    break;
                case ButtonPress:
                    btn_pressed = 1;
                    rx = ev.xbutton.x;
                    ry = ev.xbutton.y;
                    break;
                case ButtonRelease:
                    done = 1;
                    break;
            }
        }
    }
    /* clear the drawn rectangle */
    if (rect_w) {
        // XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
        XFlush(disp);
    }
    rw = ev.xbutton.x - rx;
    rh = ev.xbutton.y - ry;
    /* cursor moves backwards */
    if (rw < 0) {
        rx += rw;
        rw = 0 - rw;
    }
    if (rh < 0) {
        ry += rh;
        rh = 0 - rh;
    }

    if (rw < threshold || rh < threshold) {
        video_width_ = scr->width;
        video_height_ = scr->height;
        video_offset_x_ = 0;
        video_offset_y_ = 0;
    } else {
        video_width_ = rw;
        video_height_ = rh;
        video_offset_x_ = rx;
        video_offset_y_ = ry;
    }

    XCloseDisplay(disp);

#else
    video_width_ = 1920;
    video_height_ = 1080;
    video_offset_x_ = video_offset_y_ = 0;
#endif

    return 0;
}