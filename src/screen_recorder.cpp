#include "../include/screen_recorder.h"

#include <stdio.h>
#include <stdlib.h>

#include <sstream>
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

#include "../include/duration_logger.h"

/* initialize the resources*/
ScreenRecorder::ScreenRecorder() {
    video_framerate_ = 30;
    out_video_pix_fmt_ = AV_PIX_FMT_YUV420P;
    out_video_codec_id_ = AV_CODEC_ID_H264;
    out_audio_codec_id_ = AV_CODEC_ID_AAC;

    video_enc_options_.insert({"preset", "ultrafast"});

    in_fmt_name_ = "avfoundation";
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder() {
    if (recorder_thread_.joinable() == true) {
        recorder_thread_.join();
    }

    /* TO-DO: free all data structures */
}

void ScreenRecorder::Start(const std::string &output_file, bool audio) {
    output_file_ = output_file;
    stop_capture_ = false;
    paused_ = false;
    record_audio_ = audio;
    std::stringstream ss;

    avdevice_register_all();

    try {
        if (SelectArea()) {
            std::cerr << "Error in SelectArea" << std::endl;
            exit(1);
        }

        // #ifdef __linux__
        //     char video_device_name[20];
        //     char *display = getenv("DISPLAY");
        //     sprintf(video_device_name, "%s.0+%d,%d", display, offset_x_, offset_y_);
        //     OpenInputDevice(in_fmt_ctx_, av_find_input_format("x11grab"), video_device_name, &video_device_options_);
        //     if (record_audio_) OpenInputDevice(in_audio_fmt_ctx_, av_find_input_format("pulse"), "default", NULL);
        // #else
        //     ss << "1:";
        //     if (record_audio_) ss << "0";

        ss << "1:";
        if (record_audio_) ss << "0";

        auto demux_options = std::map<std::string, std::string>();
        demux_options.insert({"video_size", "1920x1080"});
        demux_options.insert({"framerate", "30"});
        demux_options.insert({"capture_cursor", "1"});

        demuxer_ = std::unique_ptr<Demuxer>(new Demuxer(in_fmt_name_, ss.str(), demux_options));
        muxer_ = std::unique_ptr<Muxer>(new Muxer(output_file_));

        video_dec_ = std::unique_ptr<Decoder>(new Decoder(demuxer_->getVideoParams()));
        if (record_audio_) {
            audio_dec_ = std::unique_ptr<Decoder>(new Decoder(demuxer_->getAudioParams()));
        }

        video_enc_ = std::shared_ptr<VideoEncoder>(
            new VideoEncoder(out_video_codec_id_, video_enc_options_, muxer_->getGlobalHeaderFlags(),
                             demuxer_->getVideoParams(), out_video_pix_fmt_, video_framerate_));
        if (record_audio_) {
            audio_enc_ = std::shared_ptr<AudioEncoder>(new AudioEncoder(
                out_audio_codec_id_, audio_enc_options_, muxer_->getGlobalHeaderFlags(), demuxer_->getAudioParams()));
        }

        muxer_->addVideoStream(video_enc_->getCodecContext());
        if (record_audio_) {
            muxer_->addAudioStream(audio_enc_->getCodecContext());
        }

        video_conv_ = std::unique_ptr<VideoConverter>(
            new VideoConverter(video_dec_->getCodecContext(), video_enc_->getCodecContext()));

        demuxer_->dumpInfo();
        muxer_->dumpInfo();

        muxer_->openFile();

        recorder_thread_ = std::thread([this]() {
            std::cout << "Recording..." << std::endl;
            this->CaptureFrames();
        });

    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << ", terminating..." << std::endl;
        exit(1);
    }
}

void ScreenRecorder::Stop() {
    {
        std::unique_lock<std::mutex> ul{mutex_};
        this->stop_capture_ = true;
        this->paused_ = false;
        cv_.notify_all();
    }

    if (recorder_thread_.joinable() == true) {
        recorder_thread_.join();
    }

    muxer_->closeFile();
}

void ScreenRecorder::Pause() {
    std::unique_lock<std::mutex> ul{mutex_};
    this->paused_ = true;
    std::cout << "Recording paused" << std::endl;
    cv_.notify_all();
}

void ScreenRecorder::Resume() {
    std::unique_lock<std::mutex> ul{mutex_};
    this->paused_ = false;
    std::cout << "Recording resumed" << std::endl;
    cv_.notify_all();
}

int ScreenRecorder::InitAudioConverter() {
    int ret;
    int fifo_duration = 2;  // How many seconds of audio to store in the FIFO buffer
    auto in_audio_codec_ctx = audio_dec_->getCodecContext();
    auto out_audio_codec_ctx = audio_enc_->getCodecContext();
    auto in_audio_params = demuxer_->getAudioParams();

    audio_converter_ctx_ = swr_alloc_set_opts(
        nullptr, av_get_default_channel_layout(in_audio_codec_ctx->channels), out_audio_codec_ctx->sample_fmt,
        in_audio_codec_ctx->sample_rate, av_get_default_channel_layout(in_audio_codec_ctx->channels),
        (AVSampleFormat)in_audio_params->format, in_audio_params->sample_rate, 0, nullptr);

    if (!audio_converter_ctx_) {
        std::cerr << "Error allocating audio converter";
        return -1;
    }

    ret = swr_init(audio_converter_ctx_);
    if (ret < 0) {
        std::cerr << "Error initializing audio FIFO buffer";
        return -1;
    }

    audio_fifo_buf_ = av_audio_fifo_alloc(out_audio_codec_ctx->sample_fmt, in_audio_codec_ctx->channels,
                                          in_audio_codec_ctx->sample_rate * fifo_duration);

    if (!audio_converter_ctx_) {
        std::cerr << "Error allocating audio converter";
        return -1;
    }

    return 0;
}

int ScreenRecorder::WriteAudioFrameToFifo(AVFrame *frame) {
    int ret;
    auto out_audio_codec_ctx = audio_enc_->getCodecContext();
    uint8_t **buf = nullptr;

    ret = av_samples_alloc_array_and_samples(&buf, NULL, out_audio_codec_ctx->channels, frame->nb_samples,
                                             out_audio_codec_ctx->sample_fmt, 0);
    if (ret < 0) {
        throw std::runtime_error("Fail to alloc samples by av_samples_alloc_array_and_samples.");
    }

    ret = swr_convert(audio_converter_ctx_, buf, frame->nb_samples, (const uint8_t **)frame->extended_data,
                      frame->nb_samples);
    if (ret < 0) {
        throw std::runtime_error("Fail to swr_convert.");
    }

    if (av_audio_fifo_space(audio_fifo_buf_) < frame->nb_samples)
        throw std::runtime_error("audio buffer is too small.");

    ret = av_audio_fifo_write(audio_fifo_buf_, (void **)buf, frame->nb_samples);
    if (ret < 0) {
        throw std::runtime_error("Fail to write fifo");
    }

    av_freep(&buf[0]);

    return 0;
}

int ScreenRecorder::EncodeWriteFrame(AVFrame *frame, int audio_video) {
    int ret;
    AVPacket *packet;
    const AVCodecContext *codec_ctx;
    std::shared_ptr<Encoder> encoder;
    int stream_index;

    if (audio_video) {
        encoder = audio_enc_;
        stream_index = muxer_->getAudioStream()->index;
    } else {
        encoder = video_enc_;
        stream_index = muxer_->getVideoStream()->index;
    }

    packet = av_packet_alloc();
    if (!packet) {
        std::cerr << "Could not allocate inPacket" << std::endl;
        return -1;
    }

    try {
        encoder->sendFrame(frame);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    while (true) {
        try {
            if (!encoder->fillPacket(packet)) break;
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
            return -1;
        }

        packet->stream_index = stream_index;

        muxer_->writePacket(packet);
    }

    av_packet_free(&packet);

    return 0;
}

int ScreenRecorder::ProcessVideoPkt(AVPacket *packet) {
    AVFrame *in_frame;
    AVFrame *out_frame;
    auto out_video_codec_ctx = video_enc_->getCodecContext();
    auto out_video_stream = muxer_->getVideoStream();
    DurationLogger dl(" processed in ");

    in_frame = av_frame_alloc();
    if (!in_frame) {
        std::cerr << "\nunable to release the avframe resources" << std::endl;
        return -1;
    }

    out_frame = video_conv_->allocFrame();

    try {
        video_dec_->sendPacket(packet);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    while (true) {
        try {
            if (!video_dec_->fillFrame(in_frame)) break;
        } catch (const std::runtime_error &e) {
            std::cerr << e.what() << std::endl;
            return -1;
        }

        video_conv_->convertFrame(in_frame, out_frame);

        out_frame->pts =
            av_rescale_q(video_frame_counter_++, out_video_codec_ctx->time_base, out_video_stream->time_base);

        if (EncodeWriteFrame(out_frame, 0)) return -1;
    }

    av_frame_free(&in_frame);

    video_conv_->freeFrame(&out_frame);

    return 0;
}

int ScreenRecorder::ProcessAudioPkt(AVPacket *packet) {
    int ret;
    AVFrame *in_frame;
    AVFrame *out_frame;
    auto in_audio_codec_ctx = audio_dec_->getCodecContext();
    auto out_audio_codec_ctx = audio_enc_->getCodecContext();
    auto out_audio_stream = muxer_->getAudioStream();
    DurationLogger dl(" processed in ");

    in_frame = av_frame_alloc();
    if (!in_frame) {
        std::cerr << "\nunable to release the avframe resources" << std::endl;
        return -1;
    }

    try {
        audio_dec_->sendPacket(packet);
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    while (true) {
        try {
            if (!audio_dec_->fillFrame(in_frame)) break;
        } catch (const std::runtime_error &e) {
            std::cerr << e.what() << std::endl;
            break;
        }

        ret = WriteAudioFrameToFifo(in_frame);
        if (ret < 0) {
            throw std::runtime_error("can not write in audio FIFO buffer");
        }

        while (av_audio_fifo_size(audio_fifo_buf_) >= out_audio_codec_ctx->frame_size) {
            out_frame = av_frame_alloc();
            if (!out_frame) {
                std::cerr << "Could not allocate audio out_frame" << std::endl;
                return -1;
            }

            out_frame->nb_samples = out_audio_codec_ctx->frame_size;
            out_frame->channels = in_audio_codec_ctx->channels;
            out_frame->channel_layout = av_get_default_channel_layout(in_audio_codec_ctx->channels);
            out_frame->format = out_audio_codec_ctx->sample_fmt;
            out_frame->sample_rate = out_audio_codec_ctx->sample_rate;
            out_frame->pts = av_rescale_q(out_audio_codec_ctx->frame_size * audio_frame_counter_++,
                                          out_audio_codec_ctx->time_base, out_audio_stream->time_base);

            ret = av_frame_get_buffer(out_frame, 0);
            if (ret < 0) {
                std::cerr << "Cannot fill out_frame buffers";
                return -1;
            }

            ret = av_audio_fifo_read(audio_fifo_buf_, (void **)out_frame->data, out_audio_codec_ctx->frame_size);
            if (ret < 0) {
                std::cerr << "Cannot read from audio FIFO";
                return -1;
            }

            if (EncodeWriteFrame(out_frame, 1)) return -1;

            av_frame_free(&out_frame);
        }
    }

    av_frame_free(&in_frame);

    return 0;
}

int ScreenRecorder::FlushEncoders() {
    if (EncodeWriteFrame(NULL, 0)) return -1;
    if (record_audio_ && EncodeWriteFrame(NULL, 1)) return -1;
    return 0;
}

/* function to capture and store data in frames by allocating required memory and auto deallocating the memory.   */
int ScreenRecorder::CaptureFrames() {
    /*
     * When you decode a single packet, you still don't have information enough to have a frame
     * [depending on the type of codec, some of them you do], when you decode a GROUP of packets
     * that represents a frame, then you have a picture! that's why frame_finished
     * will let you know you decoded enough to have a frame.
     */
    int ret;
    int video_pkt_counter = 0;
    int audio_pkt_counter = 0;
    AVPacket *packet;
#ifdef __linux__
    AVPacket *audio_packet;
    bool video_data_present = false;
    bool audio_data_present = false;
#endif

    // if (InitVideoConverter()) exit(1);
    if (record_audio_ && InitAudioConverter()) exit(1);

    /* start counting for PTS */
    video_frame_counter_ = 0;
    if (record_audio_) audio_frame_counter_ = 0;

    packet = av_packet_alloc();
    if (!packet) {
        std::cerr << "Could not allocate packet";
        exit(1);
    }

#ifdef __linux__
    if (record_audio_) {
        audio_packet = av_packet_alloc();
        if (!packet) {
            std::cerr << "Could not allocate in_audio_packet";
            exit(1);
        }
    }
#endif

    while (true) {
        std::unique_lock<std::mutex> ul{mutex_};
        cv_.wait(ul, [this]() { return !paused_; });
        if (stop_capture_) {
            break;
        }

#ifdef __linux__

        ret = av_read_frame(in_fmt_ctx_, packet);
        if (ret == AVERROR(EAGAIN)) {
            video_data_present = false;
        } else if (ret < 0) {
            std::cerr << "ERROR: Cannot read frame" << std::endl;
            exit(1);
        } else {
            video_data_present = true;
        }

        if (record_audio_) {
            ret = av_read_frame(in_audio_fmt_ctx_, audio_packet);
            if (ret == AVERROR(EAGAIN)) {
                audio_data_present = false;
            } else if (ret < 0) {
                std::cerr << "ERROR: Cannot read frame" << std::endl;
                exit(1);
            } else {
                audio_data_present = true;
            }
        }

        if (video_data_present) {
            std::cout << "[V] packet " << video_pkt_counter++;
            if (ProcessVideoPkt(packet)) exit(1);
            av_packet_unref(packet);
        }

        if (audio_data_present) {
            std::cout << std::endl << "[A] packet " << audio_pkt_counter++;
            if (ProcessAudioPkt(audio_packet)) exit(1);
            av_packet_unref(audio_packet);
        }

#else  // macOS

        try {
            if (!demuxer_->fillPacket(packet)) continue;
        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
            exit(1);
        }

        if (packet->stream_index == demuxer_->getVideoStreamIdx()) {
            std::cout << "[V] packet " << video_pkt_counter++;
            if (ProcessVideoPkt(packet)) exit(1);
        } else if (record_audio_ && (packet->stream_index == demuxer_->getAudioStreamIdx())) {
            std::cout << "[A] packet " << audio_pkt_counter++;
            if (ProcessAudioPkt(packet)) exit(1);
        } else {
            std::cout << "unknown packet (index: " << packet->stream_index << "), ignoring...";
        }

        av_packet_unref(packet);

#endif

        std::cout << std::endl;
    }

    av_packet_free(&packet);
#ifdef __linux__
    if (record_audio_) av_packet_free(&audio_packet);
#endif

    if (FlushEncoders()) {
        std::cerr << "ERROR: Could not flush encoders" << std::endl;
        exit(1);
    };

    return 0;
}

int ScreenRecorder::SelectArea() {
#ifdef __linux__
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
        width_ = scr->width;
        height_ = scr->height;
        offset_x_ = 0;
        offset_y_ = 0;
    } else {
        width_ = rw;
        height_ = rh;
        offset_x_ = rx;
        offset_y_ = ry;
    }

    XCloseDisplay(disp);

#else
    width_ = 1920;
    height_ = 1080;
    offset_x_ = offset_y_ = 0;
#endif

    std::cout << "Size: " << width_ << "x" << height_ << std::endl;
    std::cout << "Offset: " << offset_x_ << "," << offset_y_ << std::endl;

    return 0;
}