#include "../include/screen_recorder.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

using namespace std;

static int PrepareCodecCtx(AVCodec *&codec, AVCodecContext *&codec_ctx, AVCodecParameters *codec_params) {
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
ScreenRecorder::ScreenRecorder()
    : video_stream_idx_(-1), audio_stream_idx_(-1), video_framerate_(60), audio_samplerate_(48000) {
    // av_register_all();
    // avcodec_register_all();
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
}

int ScreenRecorder::PrepareVideoEncoder() {
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
    out_video_codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    out_video_codec_ctx_->width = in_video_codec_ctx_->width;
    out_video_codec_ctx_->height = in_video_codec_ctx_->height;
    out_video_codec_ctx_->framerate = (AVRational){video_framerate_, 1};
    out_video_codec_ctx_->time_base.num = 1;
    out_video_codec_ctx_->time_base.den = video_framerate_;

    out_video_stream_->time_base = out_video_codec_ctx_->time_base;

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

int ScreenRecorder::PrepareAudioEncoder() {
    int ret;
    AVStream *in_stream = in_fmt_ctx_->streams[audio_stream_idx_];
    int channels;

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

    channels = in_stream->codecpar->channels;
    if (!channels) channels = 2;

    out_audio_codec_ctx_ = avcodec_alloc_context3(out_audio_codec_);
    out_audio_codec_ctx_->channels = channels;
    out_audio_codec_ctx_->channel_layout = av_get_default_channel_layout(channels);
    out_audio_codec_ctx_->sample_rate = in_stream->codecpar->sample_rate;
    out_audio_codec_ctx_->sample_fmt = out_audio_codec_->sample_fmts[0];  // for aac , there is AV_SAMPLE_FMT_FLTP =8
    out_audio_codec_ctx_->bit_rate = 32000;
    out_audio_codec_ctx_->time_base.num = 1;
    out_audio_codec_ctx_->time_base.den = out_audio_codec_ctx_->sample_rate;

    out_audio_stream_->time_base = out_audio_codec_ctx_->time_base;

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

/* establishing the connection between camera or screen through its respective folder */
int ScreenRecorder::OpenInputDevices() {
    int ret;
    video_options_ = NULL;
    in_fmt_ctx_ = NULL;
    char str[20];
    char device_name[20];
    AVInputFormat *in_fmt;
    AVCodecParameters *video_codec_params;
    AVCodecParameters *audio_codec_params;

#ifdef __linux__
    in_fmt = av_find_input_format("x11grab");
    sprintf(device_name, ":0.0+%d,%d", offset_x_, offset_y_);
#else  // macOS
    in_fmt = av_find_input_format("avfoundation");
    sprintf(device_name, "1:0");
#endif

    sprintf(str, "%d", video_framerate_);
    ret = av_dict_set(&video_options_, "framerate", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }

    ret = av_dict_set(&video_options_, "show_region", "1", 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }

    sprintf(str, "%dx%d", width_, height_);
    ret = av_dict_set(&video_options_, "video_size", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }

    cout << "\nSize: " << width_ << "x" << height_ << endl;
    cout << "\nOffset: " << offset_x_ << " " << offset_y_ << endl;

    ret = avformat_open_input(&in_fmt_ctx_, device_name, in_fmt, &video_options_);
    if (ret != 0) {
        cerr << "\nerror in opening input device";
        exit(1);
    }

    ret = avformat_find_stream_info(in_fmt_ctx_, NULL);
    if (ret < 0) {
        cerr << "\nunable to find the stream information";
        exit(1);
    }

    /* find the first video stream index . Also there is an API available to do the below operations */
    for (int i = 0; i < in_fmt_ctx_->nb_streams; i++) {
        if (in_fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx_ = i;
        } else if (in_fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx_ = i;
        }
    }

    if (video_stream_idx_ == -1) {
        cout << "\nunable to find the video stream index. (-1)";
        exit(1);
    }

    if (audio_stream_idx_ == -1) {
        cout << "\nunable to find the audio stream index. (-1)";
        exit(1);
    }

    video_codec_params = in_fmt_ctx_->streams[video_stream_idx_]->codecpar;
    audio_codec_params = in_fmt_ctx_->streams[audio_stream_idx_]->codecpar;

    ret = PrepareCodecCtx(in_video_codec_, in_video_codec_ctx_, video_codec_params);
    if (ret < 0) {
        exit(1);
    }

    ret = PrepareCodecCtx(in_audio_codec_, in_audio_codec_ctx_, audio_codec_params);
    if (ret < 0) {
        exit(1);
    }

    av_dump_format(in_fmt_ctx_, 0, device_name, 0);

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

    PrepareVideoEncoder();
    PrepareAudioEncoder();

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

/* function to capture and store data in frames by allocating required memory and auto deallocating the memory.   */
int ScreenRecorder::CaptureFrames() {
    /*
     * When you decode a single packet, you still don't have information enough to have a frame
     * [depending on the type of codec, some of them you do], when you decode a GROUP of packets
     * that represents a frame, then you have a picture! that's why frameFinished
     * will let you know you decoded enough to have a frame.
     */
    int frameFinished;
    int frameIdx = 0;
    int flag;
    int ret;

    /* Compressed (encoded) video data */
    AVPacket *packet;
    /* Decoded video data (input) */
    AVFrame *inFrame;
    /* Decoded video data (output) */
    AVFrame *outFrame;

    packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(packet);

    inFrame = av_frame_alloc();
    if (!inFrame) {
        cout << "\nunable to release the avframe resources";
        exit(1);
    }

    /*
     * Since we're planning to output PPM files, which are stored in 24-bit RGB,
     * we're going to have to convert our frame from its native format to RGB.
     * ffmpeg will do these conversions for us. For most projects (including ours)
     * we're going to want to convert our initial frame to a specific format.
     * Let's allocate a frame for the converted frame now.
     */
    outFrame = av_frame_alloc();  // Allocate an AVFrame and set its fields to default values.
    if (!outFrame) {
        cout << "\nunable to release the avframe resources for outframe";
        exit(1);
    }

    int video_outbuf_size;
    int nbytes = av_image_get_buffer_size(out_video_codec_ctx_->pix_fmt, out_video_codec_ctx_->width,
                                          out_video_codec_ctx_->height, 32);
    uint8_t *video_outbuf = (uint8_t *)av_malloc(nbytes);
    if (video_outbuf == NULL) {
        cout << "\nunable to allocate memory";
        exit(1);
    }

    // Setup the data pointers and linesizes based on the specified image parameters and the provided array.
    ret = av_image_fill_arrays(outFrame->data, outFrame->linesize, video_outbuf, AV_PIX_FMT_YUV420P,
                               out_video_codec_ctx_->width, out_video_codec_ctx_->height,
                               1);  // returns : the size in bytes required for src
    if (ret < 0) {
        cout << "\nerror in filling image array";
    }

    SwsContext *swsCtx_;

    // Allocate and return swsContext.
    // a pointer to an allocated context, or NULL in case of error
    // Deprecated : Use sws_getCachedContext() instead.
    swsCtx_ = sws_getContext(in_video_codec_ctx_->width, in_video_codec_ctx_->height, in_video_codec_ctx_->pix_fmt,
                             out_video_codec_ctx_->width, out_video_codec_ctx_->height, out_video_codec_ctx_->pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);

    int ii = 0;
    int no_frames = 500;
    // cout << "\nenter No. of frames to capture : ";
    // cin >> no_frames;

    AVPacket outPacket;
    int j = 0;

    while (ii < no_frames) {
        ret = av_read_frame(in_fmt_ctx_, packet);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN)) continue;
            cerr << "ERROR: Cannot read frame!" << endl;
            exit(1);
        }

        ii++;
        cout << "Read packet " << ii << endl;

        if (packet->stream_index == video_stream_idx_) {
            ret = avcodec_send_packet(in_video_codec_ctx_, packet);
            if (ret < 0) {
                fprintf(stderr, "Error sending a packet for decoding\n");
                exit(1);
            }

            while (true) {
                ret = avcodec_receive_frame(in_video_codec_ctx_, inFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error during decoding\n");
                    exit(1);
                }

                // Convert the image from input (set in OpenInputDevices) to output format (set in OpenOutputFile)
                sws_scale(swsCtx_, inFrame->data, inFrame->linesize, 0, in_video_codec_ctx_->height, outFrame->data,
                          outFrame->linesize);
                av_init_packet(&outPacket);
                outPacket.data = NULL;  // packet data will be allocated by the encoder
                outPacket.size = 0;

                outFrame->format = out_video_codec_ctx_->pix_fmt;
                outFrame->width = out_video_codec_ctx_->width;
                outFrame->height = out_video_codec_ctx_->height;

                // we send a frame to the encoder
                ret = avcodec_send_frame(out_video_codec_ctx_, outFrame);
                if (ret < 0) {
                    fprintf(stderr, "Error sending a frame for encoding\n");
                    exit(1);
                }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(out_video_codec_ctx_, &outPacket);
                    if (ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret == AVERROR_EOF) {
                        return 0;
                    } else if (ret < 0) {
                        fprintf(stderr, "Error during encoding\n");
                        exit(1);
                    }

                    if (outPacket.size > 0) {
                        if (outPacket.pts != AV_NOPTS_VALUE)
                            outPacket.pts = av_rescale_q(outPacket.pts, out_video_codec_ctx_->time_base,
                                                         out_video_stream_->time_base);
                        if (outPacket.dts != AV_NOPTS_VALUE)
                            outPacket.dts = av_rescale_q(outPacket.dts, out_video_codec_ctx_->time_base,
                                                         out_video_stream_->time_base);

                        printf("Write frame %3d (size= %2d)\n", j++, outPacket.size / 1000);
                        if (av_write_frame(out_fmt_ctx_, &outPacket) != 0) {
                            cout << "\nerror in writing video frame";
                        }

                        av_packet_unref(&outPacket);
                    }
                }

                // TO-DO: check if this is required
                av_packet_unref(&outPacket);
            }  // frameFinished
        }
    }  // End of while-loop

    ret = av_write_trailer(out_fmt_ctx_);
    if (ret < 0) {
        cout << "\nerror in writing av trailer";
        exit(1);
    }

    // THIS WAS ADDED LATER
    av_free(video_outbuf);
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
        width = scr->width;
        height = scr->height;
        offset_x_ = 0;
        offset_y_ = 0;
    } else {
        width = rw;
        height = rh;
        offset_x_ = rx;
        offset_y_ = ry;
    }

    XCloseDisplay(disp);

    return EXIT_SUCCESS;
#else
    width_ = 1920;
    height_ = 1080;
    offset_x_ = offset_y_ = 0;
#endif
}