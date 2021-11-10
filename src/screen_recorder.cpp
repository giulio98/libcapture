#include "../include/screen_recorder.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

using namespace std;

static int InitCodecCtx(AVCodecContext *&codec_ctx, AVCodec *&codec, AVCodecParameters *codec_params) {
    int ret;

    codec = avcodec_find_decoder(codec_params->codec_id);
    if (codec == NULL) {
        cerr << "\nunable to find the video decoder";
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        cerr << "\nfailed to allocated memory for AVCodecContext";
        return -1;
    }

    // Fill the codec context based on the values from the supplied codec parameters
    // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        cerr << "\nfailed to copy codec params to codec context";
        return -1;
    }

    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        cerr << "\nunable to open the av codec";
        return -1;
    }

    return 0;
}

/* initialize the resources*/
ScreenRecorder::ScreenRecorder() {
#ifdef __linux__
    /* x11grab has some issues doing more than 30 fps */
    video_framerate_ = 30;
#else
    video_framerate_ = 60;
#endif
    video_pix_fmt_ = AV_PIX_FMT_YUV420P;
    avdevice_register_all();
    cout << "\nall required functions are registered successfully";
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder() {
    avformat_close_input(&in_fmt_ctx_);
    if (!in_fmt_ctx_) {
        cout << "\nfile closed sucessfully";
    } else {
        cout << "\nunable to close the file";
        exit(1);
    }

    avformat_free_context(in_fmt_ctx_);
    if (!in_fmt_ctx_) {
        cout << "\navformat free successfully";
    } else {
        cout << "\nunable to free avformat context";
        exit(1);
    }

    /* TO-DO: free all data structures */
}

int ScreenRecorder::SetVideoOptions() {
    char str[20];
    int ret;

    sprintf(str, "%d", video_framerate_);
    ret = av_dict_set(&video_options_, "framerate", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting framerate";
        return -1;
    }

    ret = av_dict_set(&video_options_, "show_region", "1", 0);
    if (ret < 0) {
        cout << "\nerror in setting show_region";
        return -1;
    }

    sprintf(str, "%dx%d", width_, height_);
    ret = av_dict_set(&video_options_, "video_size", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting video_size";
        return -1;
    }

    return 0;
}

int ScreenRecorder::InitVideoConverter() {
    video_converter_ctx_ =
        sws_getContext(in_video_codec_ctx_->width, in_video_codec_ctx_->height, in_video_codec_ctx_->pix_fmt,
                       out_video_codec_ctx_->width, out_video_codec_ctx_->height, out_video_codec_ctx_->pix_fmt,
                       SWS_BICUBIC, NULL, NULL, NULL);

    if (!video_converter_ctx_) {
        cerr << "Cannot allocate video_converter_ctx_";
        return -1;
    }

    return 0;
}

int ScreenRecorder::InitAudioConverter() {
    int ret;
    int fifo_duration = 2;  // How many seconds of audio to store in the FIFO buffer
    AVStream *in_stream;

#ifdef __linux__
    in_stream = in_audio_fmt_ctx_->streams[in_audio_stream_idx_];
#else
    in_stream = in_fmt_ctx_->streams[in_audio_stream_idx_];
#endif

    audio_converter_ctx_ = swr_alloc_set_opts(
        nullptr, av_get_default_channel_layout(in_audio_codec_ctx_->channels), out_audio_codec_ctx_->sample_fmt,
        in_audio_codec_ctx_->sample_rate, av_get_default_channel_layout(in_audio_codec_ctx_->channels),
        (AVSampleFormat)in_stream->codecpar->format, in_stream->codecpar->sample_rate, 0, nullptr);

    if (!audio_converter_ctx_) {
        cerr << "Error allocating audio converter";
        return -1;
    }

    ret = swr_init(audio_converter_ctx_);
    if (ret < 0) {
        cerr << "Error initializing audio FIFO buffer";
        return -1;
    }

    audio_fifo_buf_ = av_audio_fifo_alloc(out_audio_codec_ctx_->sample_fmt, in_audio_codec_ctx_->channels,
                                          in_audio_codec_ctx_->sample_rate * fifo_duration);

    if (!audio_converter_ctx_) {
        cerr << "Error allocating audio converter";
        return -1;
    }

    return 0;
}

/* establishing the connection between camera or screen through its respective folder */
int ScreenRecorder::OpenInputDevice(AVFormatContext *&in_fmt_ctx, AVInputFormat *in_fmt, const char *device_name,
                                    AVDictionary **options) {
    int ret;
    in_fmt_ctx = NULL;

    ret = avformat_open_input(&in_fmt_ctx, device_name, in_fmt, options);
    if (ret != 0) {
        cerr << "\nerror in opening input device";
        exit(1);
    }

    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        cerr << "\nunable to find the stream information";
        exit(1);
    }

    for (int i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream *stream = in_fmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            in_video_stream_idx_ = i;
            if (InitCodecCtx(in_video_codec_ctx_, in_video_codec_, stream->codecpar)) {
                cerr << "Cannot Initialize in_video_codec_ctx";
                exit(1);
            }
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            in_audio_stream_idx_ = i;
            if (InitCodecCtx(in_audio_codec_ctx_, in_audio_codec_, stream->codecpar)) {
                cerr << "Cannot Initialize in_audio_codec_ctx";
                exit(1);
            }
        }
    }

    av_dump_format(in_fmt_ctx, 0, device_name, 0);

    return 0;
}

int ScreenRecorder::OpenInputDevices() {
    video_options_ = NULL;
    if (SetVideoOptions()) {
        cerr << "Error in etting video options" << endl;
        exit(1);
    };

    in_video_stream_idx_ = -1;
    in_audio_stream_idx_ = -1;

#ifdef __linux__
    OpenInputDevice(in_fmt_ctx_, av_find_input_format("x11grab"), ":1.0", &video_options_);
    OpenInputDevice(in_audio_fmt_ctx_, av_find_input_format("alsa"), "hw:0", NULL);
#else
    OpenInputDevice(in_fmt_ctx_, av_find_input_format("avfoundation"), "1:0", &video_options_);
#endif

    if (in_video_stream_idx_ == -1) {
        cout << "\nunable to find the video stream index. (-1)";
        exit(1);
    }

    if (in_audio_stream_idx_ == -1) {
        cout << "\nunable to find the audio stream index. (-1)";
        exit(1);
    }

    return 0;
}

int ScreenRecorder::InitVideoEncoder() {
    int ret;

    out_video_stream_ = avformat_new_stream(out_fmt_ctx_, NULL);
    if (!out_video_stream_) {
        cout << "\nerror in creating a av format new stream";
        return -1;
    }

    if (!out_fmt_ctx_->nb_streams) {
        cout << "\noutput file dose not contain any stream";
        return -1;
    }

    out_video_codec_ = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!out_video_codec_) {
        cout << "\nerror in finding the av codecs. try again with correct codec";
        return -1;
    }

    /* set property of the video file */
    out_video_codec_ctx_ = avcodec_alloc_context3(out_video_codec_);
    out_video_codec_ctx_->pix_fmt = video_pix_fmt_;
    out_video_codec_ctx_->width = in_video_codec_ctx_->width;
    out_video_codec_ctx_->height = in_video_codec_ctx_->height;
    out_video_codec_ctx_->framerate = (AVRational){video_framerate_, 1};
    out_video_codec_ctx_->time_base.num = 1;
    out_video_codec_ctx_->time_base.den = video_framerate_;

    /*
     * Some container formats (like MP4) require global headers to be present
     * Mark the encoder so that it behaves accordingly.
     */
    if (out_fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        out_video_codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(out_video_codec_ctx_, out_video_codec_, NULL);
    if (ret < 0) {
        cout << "\nerror in opening the avcodec";
        return -1;
    }

    ret = avcodec_parameters_from_context(out_video_stream_->codecpar, out_video_codec_ctx_);
    if (ret < 0) {
        cout << "\nerror in writing video stream parameters";
        return -1;
    }

    return 0;
}

int ScreenRecorder::InitAudioEncoder() {
    int ret;
    AVStream *in_stream;

#ifdef __linux__
    in_stream = in_audio_fmt_ctx_->streams[in_audio_stream_idx_];
#else
    in_stream = in_fmt_ctx_->streams[in_audio_stream_idx_];
#endif

    out_audio_stream_ = avformat_new_stream(out_fmt_ctx_, NULL);
    if (!out_video_stream_) {
        cout << "\nerror in creating a av format new stream";
        exit(1);
    }

    if (!out_fmt_ctx_->nb_streams) {
        cout << "\noutput file dose not contain any stream";
        return -1;
    }

    out_audio_codec_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!out_audio_codec_) {
        cout << "\nerror in finding the av codecs. try again with correct codec";
        return -1;
    }

    // #ifdef __linux__
    //     sample_rate = 48000;
    //     channels = 2;
    // #else
    //     sample_rate = in_stream->codecpar->sample_rate;
    //     channels = in_stream->codecpar->channels;
    // #endif

    out_audio_codec_ctx_ = avcodec_alloc_context3(out_audio_codec_);
    out_audio_codec_ctx_->channels = in_stream->codecpar->channels;
    out_audio_codec_ctx_->channel_layout = av_get_default_channel_layout(in_stream->codecpar->channels);
    out_audio_codec_ctx_->sample_rate = in_stream->codecpar->sample_rate;
    out_audio_codec_ctx_->sample_fmt = out_audio_codec_->sample_fmts[0];  // for aac there is AV_SAMPLE_FMT_FLTP = 8
    out_audio_codec_ctx_->bit_rate = 96000;
    out_audio_codec_ctx_->time_base.num = 1;
    out_audio_codec_ctx_->time_base.den = out_audio_codec_ctx_->sample_rate;

    /*
     * Some container formats (like MP4) require global headers to be present
     * Mark the encoder so that it behaves accordingly.
     */
    if (out_fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
        out_audio_codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(out_audio_codec_ctx_, out_audio_codec_, NULL);
    if (ret < 0) {
        cout << "\nerror in opening the avcodec";
        return -1;
    }

    ret = avcodec_parameters_from_context(out_audio_stream_->codecpar, out_audio_codec_ctx_);
    if (ret < 0) {
        cout << "\nerror in writing video stream parameters";
        return -1;
    }

    return 0;
}

/* initialize the video output file and its properties  */
int ScreenRecorder::InitOutputFile() {
    out_fmt_ctx_ = NULL;
    output_file_ = "./media/output.mp4";
    int ret;

    /* allocate out_fmt_ctx_ */
    ret = avformat_alloc_output_context2(&out_fmt_ctx_, NULL, NULL, output_file_);
    if (ret < 0) {
        cout << "\nerror in allocating av format output context";
        exit(1);
    }

    if (InitVideoEncoder()) exit(1);
    if (InitAudioEncoder()) exit(1);

    /* create empty video file */
    if (!(out_fmt_ctx_->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_fmt_ctx_->pb, output_file_, AVIO_FLAG_WRITE) < 0) {
            cout << "\nerror in creating the output file";
            exit(1);
        }
    }

    /* imp: mp4 container or some advanced container file required header information */
    ret = avformat_write_header(out_fmt_ctx_, &video_options_);
    if (ret < 0) {
        cout << "\nerror in writing the header context";
        exit(1);
    }

    /* show complete information */
    av_dump_format(out_fmt_ctx_, 0, output_file_, 1);

    return 0;
}

int ScreenRecorder::ConvertEncodeStoreVideoPkt(AVPacket *in_packet) {
    int ret;
    AVPacket *out_packet;
    AVFrame *in_frame;
    AVFrame *out_frame;

    out_packet = av_packet_alloc();
    if (!out_packet) {
        cerr << "Could not allocate inPacket" << endl;
        return -1;
    }

    in_frame = av_frame_alloc();
    if (!in_frame) {
        cerr << "\nunable to release the avframe resources" << endl;
        return -1;
    }

    out_frame = av_frame_alloc();
    if (!out_frame) {
        cerr << "\nunable to release the avframe resources for outframe";
        return -1;
    }

    out_frame->format = out_video_codec_ctx_->pix_fmt;
    out_frame->width = out_video_codec_ctx_->width;
    out_frame->height = out_video_codec_ctx_->height;

    ret = av_image_alloc(out_frame->data, out_frame->linesize, out_frame->width, out_frame->height,
                         out_video_codec_ctx_->pix_fmt, 1);
    if (ret < 0) {
        cerr << "Failed to allocate out_frame data";
        return -1;
    }

    ret = avcodec_send_packet(in_video_codec_ctx_, in_packet);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return -1;
    }

    while (true) {
        ret = avcodec_receive_frame(in_video_codec_ctx_, in_frame);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }

        /* Convert the image from input (set in OpenInputDevices) to output format (set in OpenOutputFile) */
        sws_scale(video_converter_ctx_, in_frame->data, in_frame->linesize, 0, out_frame->height, out_frame->data,
                  out_frame->linesize);

        ret = avcodec_send_frame(out_video_codec_ctx_, out_frame);
        if (ret < 0) {
            fprintf(stderr, "Error sending a frame for encoding\n");
            exit(1);
        }

        while (true) {
            ret = avcodec_receive_packet(out_video_codec_ctx_, out_packet);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN)) {
                    break;
                } else if (ret == AVERROR_EOF) {
                    return 0;
                }
                fprintf(stderr, "Error during encoding\n");
                exit(1);
            }

            if (out_packet->pts != AV_NOPTS_VALUE)
                out_packet->pts =
                    av_rescale_q(out_packet->pts, out_video_codec_ctx_->time_base, out_video_stream_->time_base);
            if (out_packet->dts != AV_NOPTS_VALUE)
                out_packet->dts =
                    av_rescale_q(out_packet->dts, out_video_codec_ctx_->time_base, out_video_stream_->time_base);

            if (av_interleaved_write_frame(out_fmt_ctx_, out_packet) != 0) {
                cout << "\nerror in writing video frame";
            }
        }
    }

    av_packet_free(&out_packet);

    av_frame_free(&in_frame);

    av_freep(&out_frame->data[0]);  // needed beacuse of av_image_alloc() (data is not reference-counted)
    av_frame_free(&out_frame);

    return 0;
}

int ScreenRecorder::ConvertWriteAudioFifo(AVFrame *in_frame) {
    int ret;
    uint8_t **buf = nullptr;

    ret = av_samples_alloc_array_and_samples(&buf, NULL, out_audio_codec_ctx_->channels, in_frame->nb_samples,
                                             out_audio_codec_ctx_->sample_fmt, 0);
    if (ret < 0) {
        throw std::runtime_error("Fail to alloc samples by av_samples_alloc_array_and_samples.");
    }

    ret = swr_convert(audio_converter_ctx_, buf, in_frame->nb_samples, (const uint8_t **)in_frame->extended_data,
                      in_frame->nb_samples);
    if (ret < 0) {
        throw std::runtime_error("Fail to swr_convert.");
    }

    if (av_audio_fifo_space(audio_fifo_buf_) < in_frame->nb_samples)
        throw std::runtime_error("audio buffer is too small.");

    ret = av_audio_fifo_write(audio_fifo_buf_, (void **)buf, in_frame->nb_samples);
    if (ret < 0) {
        throw std::runtime_error("Fail to write fifo");
    }

    av_freep(&buf[0]);

    return 0;
}

int ScreenRecorder::ConvertEncodeStoreAudioPkt(AVPacket *in_packet) {
    int ret;
    AVPacket *out_packet;
    AVFrame *in_frame;
    AVFrame *out_frame;

    out_packet = av_packet_alloc();
    if (!out_packet) {
        cerr << "Could not allocate inPacket" << endl;
        return -1;
    }

    in_frame = av_frame_alloc();
    if (!in_frame) {
        cerr << "\nunable to release the avframe resources" << endl;
        return -1;
    }

    ret = avcodec_send_packet(in_audio_codec_ctx_, in_packet);
    if (ret < 0) {
        throw std::runtime_error("can not send pkt in decoding");
    }

    ret = avcodec_receive_frame(in_audio_codec_ctx_, in_frame);
    if (ret < 0) {
        throw std::runtime_error("can not receive frame in decoding");
    }

    ret = ConvertWriteAudioFifo(in_frame);
    if (ret < 0) {
        throw std::runtime_error("can not write in audio FIFO buffer");
    }

    while (av_audio_fifo_size(audio_fifo_buf_) >= out_audio_codec_ctx_->frame_size) {
        out_frame = av_frame_alloc();
        if (!out_frame) {
            cerr << "Could not allocate audio out_frame" << endl;
            return -1;
        }

        out_frame->nb_samples = out_audio_codec_ctx_->frame_size;
        out_frame->channels = in_audio_codec_ctx_->channels;
        out_frame->channel_layout = av_get_default_channel_layout(in_audio_codec_ctx_->channels);
        out_frame->format = out_audio_codec_ctx_->sample_fmt;
        out_frame->sample_rate = out_audio_codec_ctx_->sample_rate;

        ret = av_frame_get_buffer(out_frame, 0);
        if (ret < 0) {
            cerr << "Cannot fill out_frame buffers";
            return -1;
        }

        ret = av_audio_fifo_read(audio_fifo_buf_, (void **)out_frame->data, out_audio_codec_ctx_->frame_size);
        if (ret < 0) {
            cerr << "Cannot read from audio FIFO";
            return -1;
        }

        ret = avcodec_send_frame(out_audio_codec_ctx_, out_frame);
        if (ret < 0) {
            throw std::runtime_error("Fail to send frame in encoding");
        }

        av_frame_free(&out_frame);

        ret = avcodec_receive_packet(out_audio_codec_ctx_, out_packet);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {
            throw std::runtime_error("Fail to receive packet in encoding");
        }

        /* dts and duration of out_packet should already be set */
        out_packet->stream_index = out_audio_stream_->index;
        out_packet->pts = out_audio_codec_ctx_->frame_size * audio_pkt_counter_;

        audio_pkt_counter_++;

        ret = av_interleaved_write_frame(out_fmt_ctx_, out_packet);
        av_packet_unref(out_packet);
    }

    av_packet_free(&out_packet);
    av_frame_free(&in_frame);

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
    int in_pkt_counter = 0;
    AVPacket *in_packet;
#ifdef __linux__
    AVPacket *in_audio_packet;
    bool video_data_present;
    bool audio_data_present;
#endif
    int64_t start_time;
    int64_t current_time;
    int64_t duration = (10 * 1000 * 1000);  // 10 seconds

    if (InitVideoConverter()) exit(1);
    if (InitAudioConverter()) exit(1);

    /* necessary for audio packets PTS */
    audio_pkt_counter_ = 0;

    in_packet = av_packet_alloc();
    if (!in_packet) {
        cerr << "Could not allocate in_packet";
        exit(1);
    }

#ifdef __linux__
    in_audio_packet = av_packet_alloc();
    if (!in_packet) {
        cerr << "Could not allocate in_audio_packet";
        exit(1);
    }
#endif

    start_time = av_gettime();

    while (true) {
        current_time = av_gettime();
        if ((current_time - start_time) > duration) break;

#ifdef __linux__

        ret = av_read_frame(in_fmt_ctx_, in_packet);
        if (ret == AVERROR(EAGAIN)) {
            video_data_present = false;
        } else if (ret < 0) {
            cerr << "ERROR: Cannot read frame" << endl;
            exit(1);
        } else {
            video_data_present = true;
        }

        ret = av_read_frame(in_audio_fmt_ctx_, in_audio_packet);
        if (ret == AVERROR(EAGAIN)) {
            audio_data_present = false;
        } else if (ret < 0) {
            cerr << "ERROR: Cannot read frame" << endl;
            exit(1);
        } else {
            audio_data_present = true;
        }

        if (video_data_present) {
            cout << "Packet " << in_pkt_counter << " (video)";
            in_pkt_counter++;
            if (ConvertEncodeStoreVideoPkt(in_packet)) exit(1);
            av_packet_unref(in_packet);
        }
        cout << endl;

        if (audio_data_present) {
            cout << "Packet " << in_pkt_counter << " (audio)";
            in_pkt_counter++;
            if (ConvertEncodeStoreAudioPkt(in_audio_packet)) exit(1);
            av_packet_unref(in_audio_packet);
        }
        cout << endl;

#else  // macOS

        ret = av_read_frame(in_fmt_ctx_, in_packet);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret < 0) {
            cerr << "ERROR: Cannot read frame" << endl;
            exit(1);
        }

        cout << "Packet " << in_pkt_counter;
        in_pkt_counter++;

        if (in_packet->stream_index == in_video_stream_idx_) {
            cout << " (video)";
            if (ConvertEncodeStoreVideoPkt(in_packet)) exit(1);
        } else if (in_packet->stream_index == in_audio_stream_idx_) {
            cout << " (audio)";
            if (ConvertEncodeStoreAudioPkt(in_packet)) exit(1);
        } else {
            cout << " unknown, ignoring...";
        }

        cout << endl;
        av_packet_unref(in_packet);

#endif
    }

    av_packet_free(&in_packet);
#ifdef __linux__
    av_packet_free(&in_audio_packet);
#endif

    ret = av_write_trailer(out_fmt_ctx_);
    if (ret < 0) {
        cout << "\nerror in writing av trailer";
        exit(1);
    }

    // THIS WAS ADDED LATER
    avio_close(out_fmt_ctx_->pb);

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

    cout << "\nSize: " << width_ << "x" << height_ << endl;
    cout << "\nOffset: " << offset_x_ << " " << offset_y_ << endl;

    return 0;
}