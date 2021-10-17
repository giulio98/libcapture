#include "../include/ScreenRecorder.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>

#define ALSA_BUFFER_SIZE_MAX 132768

using namespace std;

static void video_dict_set(AVDictionary **options, const string &framerate, const string &show_region, int width,
                           int height) {
    int ret;
    char str[20];

    ret = av_dict_set(options, "framerate", framerate.c_str(), 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }

    ret = av_dict_set(options, "show_region", show_region.c_str(), 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }

    sprintf(str, "%dx%d", width, height);
    ret = av_dict_set(options, "video_size", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }
}

static void audio_dict_set(AVDictionary **options, int sample_rate, int channels, int frame_size) {
    int ret;
    char str[20];

    sprintf(str, "%d", sample_rate);
    ret = av_dict_set(options, "sample_rate", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }

    sprintf(str, "%d", channels);
    ret = av_dict_set(options, "channels", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }

    sprintf(str, "%d", frame_size);
    ret = av_dict_set(options, "frame_size", str, 0);
    if (ret < 0) {
        cout << "\nerror in setting dictionary value";
        exit(1);
    }
}

static void terminate_context(AVFormatContext **avfc) {
    avformat_close_input(avfc);
    if (!*avfc) {
        cout << "\nfile closed sucessfully";
    } else {
        cout << "\nunable to close the file";
        exit(1);
    }

    avformat_free_context(*avfc);
    if (!*avfc) {
        cout << "\navformat free successfully";
    } else {
        cout << "\nunable to free avformat context";
        exit(1);
    }
}

static int fill_stream_info(AVStream *avs, AVCodec **avc, AVCodecContext **avcc) {
    *avc = avcodec_find_decoder(avs->codecpar->codec_id);
    if (!*avc) {
        cerr << "failed to find the codec";
        exit(1);
    }

    *avcc = avcodec_alloc_context3(*avc);
    if (!*avcc) {
        cerr << "failed to alloc memory for codec context";
        exit(1);
    }

    if (avcodec_parameters_to_context(*avcc, avs->codecpar) < 0) {
        cerr << "failed to fill codec context";
        exit(1);
    }

    if (avcodec_open2(*avcc, *avc, NULL) < 0) {
        cerr << "failed to open codec";
        exit(1);
    }
    return 0;
}

int ScreenRecorder::PrepareVideoEncoder() {
    int ret;

    outVideoStream = avformat_new_stream(outFormatContext, NULL);
    if (!outVideoStream) {
        cout << "\nerror in creating a av format new stream";
        return 1;
    }

    outVideoCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!outVideoCodec) {
        cout << "\nerror in finding the av codecs. try again with correct codec";
        return 1;
    }

    /* set property of the video file */
    outVideoCodecContext = avcodec_alloc_context3(outVideoCodec);
    outVideoCodecContext->codec_id = AV_CODEC_ID_MPEG4;  // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
    outVideoCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    outVideoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    outVideoCodecContext->bit_rate = 400000;  // 2500000
    outVideoCodecContext->width = width;
    outVideoCodecContext->height = height;
    outVideoCodecContext->gop_size = 3;
    outVideoCodecContext->max_b_frames = 2;
    outVideoCodecContext->time_base.num = 1;
    outVideoCodecContext->time_base.den = 30;
    outVideoStream->time_base = outVideoCodecContext->time_base;

    if (codecId == AV_CODEC_ID_H264) {
        av_opt_set(outVideoCodecContext->priv_data, "preset", "slow", 0);
    }

    ret = avcodec_open2(outVideoCodecContext, outVideoCodec, NULL);
    if (ret < 0) {
        cout << "\nerror in opening the avcodec";
        return 1;
    }

    ret = avcodec_parameters_from_context(outVideoStream->codecpar, outVideoCodecContext);
    if (ret < 0) {
        cout << "\nerror in writing video stream parameters";
        return 1;
    }

    return 0;
}

int ScreenRecorder::PrepareAudioEncoder() {
    int ret;

    outAudioStream = avformat_new_stream(outFormatContext, NULL);
    if (!outAudioStream) {
        cout << "\nerror in creating a av format new stream";
        return 1;
    }

    outAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!outAudioCodec) {
        cout << "\nerror in finding the av codecs. try again with correct codec";
        return 1;
    }

    int OUTPUT_CHANNELS = 2;
    int SAMPLE_RATE = 48000;  // 48kHz

    /* set property of the video file */
    outAudioCodecContext = avcodec_alloc_context3(outAudioCodec);
    outAudioCodecContext->codec_id = AV_CODEC_ID_AAC;  // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
    outAudioCodecContext->codec_type = AVMEDIA_TYPE_AUDIO;
    outAudioCodecContext->channels = OUTPUT_CHANNELS;
    outAudioCodecContext->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
    outAudioCodecContext->sample_fmt = outAudioCodec->sample_fmts[0];
    outAudioCodecContext->profile = FF_PROFILE_AAC_MAIN;
    outAudioCodecContext->bit_rate = 196000;
    outAudioCodecContext->sample_rate = SAMPLE_RATE;
    outAudioCodecContext->time_base = (AVRational){1, SAMPLE_RATE};
    outAudioStream->time_base = outAudioCodecContext->time_base;

    ret = avcodec_open2(outAudioCodecContext, outAudioCodec, NULL);
    if (ret < 0) {
        cout << "\nerror in opening the avcodec";
        return 1;
    }

    // This is set to 1024, but inFrame->nb_samples is 512
    // outAudioCodecContext->frame_size = 512;

    ret = avcodec_parameters_from_context(outAudioStream->codecpar, outAudioCodecContext);
    if (ret < 0) {
        cout << "\nerror in writing video stream parameters";
        return 1;
    }

    return 0;
}

/* initialize the resources*/
ScreenRecorder::ScreenRecorder() {
    // av_register_all();
    // avcodec_register_all();
    avdevice_register_all();
    cout << "\nall required functions are registered successfully";
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder() {
    terminate_context(&inVideoFormatContext);
    terminate_context(&inAudioFormatContext);
}

/* function to capture and store data in frames by allocating required memory and auto deallocating the memory.   */
int ScreenRecorder::CaptureVideoFrames() {
    /*
     * When you decode a single packet, you still don't have information enough to have a frame
     * [depending on the type of codec, some of them you do], when you decode a GROUP of packets
     * that represents a frame, then you have a picture! that's why frameFinished
     * will let you know you decoded enough to have a frame.
     */
    // int frameFinished;
    int ret;

    /* Compressed (encoded) video data */
    AVPacket *inPacket;
    /* Decoded video data (input) */
    AVFrame *inFrame;
    /* Decoded video data (output) */
    AVFrame *outFrame;

    // DEPRECATED:
    // packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    // av_init_packet(packet);
    //
    inPacket = av_packet_alloc();
    if (!inPacket) {
        cerr << "\nunable to allocate packet";
        exit(1);
    }

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

    int nbytes = av_image_get_buffer_size(outVideoCodecContext->pix_fmt, outVideoCodecContext->width,
                                          outVideoCodecContext->height, 32);
    uint8_t *video_outbuf = (uint8_t *)av_malloc(nbytes);
    if (video_outbuf == NULL) {
        cout << "\nunable to allocate memory";
        exit(1);
    }

    // Setup the data pointers and linesizes based on the specified image parameters and the provided array.
    ret = av_image_fill_arrays(outFrame->data, outFrame->linesize, video_outbuf, AV_PIX_FMT_YUV420P,
                               outVideoCodecContext->width, outVideoCodecContext->height,
                               1);  // returns : the size in bytes required for src
    if (ret < 0) {
        cout << "\nerror in filling image array";
    }

    SwsContext *swsCtx_;

    // Allocate and return swsContext.
    // a pointer to an allocated context, or NULL in case of error
    // Deprecated : Use sws_getCachedContext() instead.
    swsCtx_ = sws_getContext(inVideoCodecContext->width, inVideoCodecContext->height, inVideoCodecContext->pix_fmt,
                             outVideoCodecContext->width, outVideoCodecContext->height, outVideoCodecContext->pix_fmt,
                             SWS_BICUBIC, NULL, NULL, NULL);

    int ii = 0;
    int no_frames = 100;
    cout << "\nenter No. of frames to capture : ";
    cin >> no_frames;

    AVPacket *outPacket;
    int j = 0;

    while (av_read_frame(inVideoFormatContext, inPacket) >= 0) {
        if (ii++ == no_frames) break;
        if (inPacket->stream_index == videoStreamIdx) {
            ret = avcodec_send_packet(inVideoCodecContext, inPacket);
            if (ret < 0) {
                fprintf(stderr, "Error sending a packet for decoding\n");
                exit(1);
            }

            while (true) {
                ret = avcodec_receive_frame(inVideoCodecContext, inFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error during decoding\n");
                    exit(1);
                }

                // Convert the image from input (set in OpenDevices) to output format (set in InitOutputFile)
                sws_scale(swsCtx_, inFrame->data, inFrame->linesize, 0, inVideoCodecContext->height, outFrame->data,
                          outFrame->linesize);
                // av_init_packet(&outPacket);
                outPacket = av_packet_alloc();
                outPacket->data = NULL;  // packet data will be allocated by the encoder
                outPacket->size = 0;

                outFrame->format = outVideoCodecContext->pix_fmt;
                outFrame->width = outVideoCodecContext->width;
                outFrame->height = outVideoCodecContext->height;

                // we send a frame to the encoder
                ret = avcodec_send_frame(outVideoCodecContext, outFrame);
                if (ret < 0) {
                    fprintf(stderr, "Error sending a frame for encoding\n");
                    exit(1);
                }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(outVideoCodecContext, outPacket);
                    if (ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret == AVERROR_EOF) {
                        return 0;
                    } else if (ret < 0) {
                        fprintf(stderr, "Error during encoding\n");
                        exit(1);
                    }

                    if (outPacket->size > 0) {
                        if (outPacket->pts != AV_NOPTS_VALUE)
                            outPacket->pts = av_rescale_q(outPacket->pts, outVideoCodecContext->time_base,
                                                          outVideoStream->time_base);
                        if (outPacket->dts != AV_NOPTS_VALUE)
                            outPacket->dts = av_rescale_q(outPacket->dts, outVideoCodecContext->time_base,
                                                          outVideoStream->time_base);

                        printf("Write frame %3d (size= %2d)\n", j++, outPacket->size / 1000);
                        if (av_write_frame(outFormatContext, outPacket) != 0) {
                            cout << "\nerror in writing video frame";
                        }

                        av_packet_unref(outPacket);
                    }
                }

                // TO-DO: check if this is required
                av_packet_unref(outPacket);
            }  // frameFinished
        }
    }  // End of while-loop

    ret = av_write_trailer(outFormatContext);
    if (ret < 0) {
        cout << "\nerror in writing av trailer";
        exit(1);
    }

    // THIS WAS ADDED LATER
    av_free(video_outbuf);
    avio_close(outFormatContext->pb);

    return 0;
}

int ScreenRecorder::CaptureAudioFrames() {
    /*
     * When you decode a single packet, you still don't have information enough to have a frame
     * [depending on the type of codec, some of them you do], when you decode a GROUP of packets
     * that represents a frame, then you have a picture! that's why frameFinished
     * will let you know you decoded enough to have a frame.
     */
    // int frameFinished;
    int ret;

    /* Compressed (encoded) video data */
    AVPacket *inPacket;
    /* Decoded video data (input) */
    AVFrame *inFrame;
    /* Decoded video data (output) */
    AVFrame *outFrame;

    // DEPRECATED:
    // packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    // av_init_packet(packet);
    //
    inPacket = av_packet_alloc();
    if (!inPacket) {
        cerr << "\nunable to allocate packet";
        exit(1);
    }

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

    int ii = 0;
    int no_frames = 100;
    cout << "\nenter No. of frames to capture : ";
    cin >> no_frames;

    AVPacket *outPacket;
    int j = 0;

    while (av_read_frame(inAudioFormatContext, inPacket) >= 0) {
        if (ii++ == no_frames) break;
        if (inPacket->stream_index == audioStreamIdx) {
            ret = avcodec_send_packet(inAudioCodecContext, inPacket);
            if (ret < 0) {
                fprintf(stderr, "Error sending a packet for decoding\n");
                exit(1);
            }

            while (true) {
                ret = avcodec_receive_frame(inAudioCodecContext, inFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    cerr << "Error during decoding\n";
                    exit(1);
                }

                outPacket = av_packet_alloc();

                // we send a frame to the encoder
                ret = avcodec_send_frame(outAudioCodecContext, inFrame);
                if (ret < 0) {
                    fprintf(stderr, "Error sending a frame for encoding\n");
                    exit(1);
                }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(outAudioCodecContext, outPacket);
                    if (ret == AVERROR(EAGAIN)) {
                        break;
                    } else if (ret == AVERROR_EOF) {
                        return 0;
                    } else if (ret < 0) {
                        fprintf(stderr, "Error during encoding\n");
                        exit(1);
                    }

                    outPacket->stream_index = videoStreamIdx + 1;

                    if (outPacket->size > 0) {
                        if (outPacket->pts != AV_NOPTS_VALUE)
                            outPacket->pts = av_rescale_q(outPacket->pts, outAudioCodecContext->time_base,
                                                          outAudioStream->time_base);
                        if (outPacket->dts != AV_NOPTS_VALUE)
                            outPacket->dts = av_rescale_q(outPacket->dts, outAudioCodecContext->time_base,
                                                          outAudioStream->time_base);

                        printf("Write frame %3d (size= %2d)\n", j++, outPacket->size / 1000);
                        if (av_interleaved_write_frame(outFormatContext, outPacket) != 0) {
                            cout << "\nerror in writing video frame";
                        }

                        av_packet_unref(outPacket);
                    }
                }

                // TO-DO: check if this is required
                av_packet_free(&outPacket);
            }  // frameFinished
        }
    }  // End of while-loop

    ret = av_write_trailer(outFormatContext);
    if (ret < 0) {
        cout << "\nerror in writing av trailer";
        exit(1);
    }

    // THIS WAS ADDED LATER
    avio_close(outFormatContext->pb);

    return 0;
}

/* establishing the connection between camera or screen through its respective folder */
int ScreenRecorder::OpenCamera() {
    int ret;
    char str[20];
    AVInputFormat *inVideoFormat;

    inVideoFormatContext = avformat_alloc_context();  // Allocate an AVFormatContext.

    /*
     * X11 video input device.
     * To enable this input device during configuration you need libxcb installed on your system. It will be
     * automatically detected during configuration. This device allows one to capture a region of an X11 display. refer
     * : https://www.ffmpeg.org/ffmpeg-devices.html#x11grab Current below is for screen recording. to connect with
     * camera use v4l2 as a input parameter for av_find_input_format
     */
    inVideoFormat = av_find_input_format("x11grab");

    /* Set the dictionary */
    videoOptions = NULL;
    video_dict_set(&videoOptions, "30", "1", width, height);

    sprintf(str, ":1.0+%d,%d", offsetX, offsetY);
    ret = avformat_open_input(&inVideoFormatContext, str, inVideoFormat, &videoOptions);
    if (ret != 0) {
        cout << "\nerror in opening input video device";
        exit(1);
    }

    ret = avformat_find_stream_info(inVideoFormatContext, NULL);
    if (ret < 0) {
        cout << "\nunable to find the stream information";
        exit(1);
    }

    videoStreamIdx = -1;
    audioStreamIdx = -1;

    /* find the first video stream index . Also there is an API available to do the below operations */
    for (int i = 0; i < inVideoFormatContext->nb_streams; i++) {
        if (inVideoFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            break;
        }
    }

    if (videoStreamIdx == -1) {
        cout << "\nunable to find the video stream index. (-1)";
        exit(1);
    }

    fill_stream_info(inVideoFormatContext->streams[videoStreamIdx], &inVideoCodec, &inVideoCodecContext);

    return 0;
}

/* establishing the connection between camera or screen through its respective folder */
int ScreenRecorder::OpenMic() {
    int ret;
    char str[20];
    AVInputFormat *inAudioFormat;

    inAudioFormatContext = avformat_alloc_context();  // Allocate an AVFormatContext.

    /*
     * X11 video input device.
     * To enable this input device during configuration you need libxcb installed on your system. It will be
     * automatically detected during configuration. This device allows one to capture a region of an X11 display. refer
     * : https://www.ffmpeg.org/ffmpeg-devices.html#x11grab Current below is for screen recording. to connect with
     * camera use v4l2 as a input parameter for av_find_input_format
     */
    inAudioFormat = av_find_input_format("alsa");
    // inAudioFormat = av_find_input_format("pulse");

    // audioOptions = NULL;
    // audio_dict_set(&audioOptions, 48000, 2, 1024);

    ret = avformat_open_input(&inAudioFormatContext, "hw:0,0", inAudioFormat, NULL);
    //ret = avformat_open_input(&inAudioFormatContext, "default", inAudioFormat, &audioOptions);
    if (ret != 0) {
        cout << "\nerror in opening input audio device";
        exit(1);
    }

    ret = avformat_find_stream_info(inAudioFormatContext, NULL);
    if (ret < 0) {
        cout << "\nunable to find the stream information";
        exit(1);
    }

    audioStreamIdx = -1;

    /* find the first video stream index . Also there is an API available to do the below operations */
    for (int i = 0; i < inAudioFormatContext->nb_streams; i++) {
        if (inAudioFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = i;
            break;
        }
    }

    if (audioStreamIdx == -1) {
        cout << "\nunable to find the audio stream index. (-1)";
        exit(1);
    }

    fill_stream_info(inAudioFormatContext->streams[audioStreamIdx], &inAudioCodec, &inAudioCodecContext);

    return 0;
}

/* initialize the video output file and its properties  */
int ScreenRecorder::InitOutputFile() {
    outFormatContext = NULL;
    int ret;
    outputFile = "../media/output.mp4";

    /* allocate outFormatContext */
    avformat_alloc_output_context2(&outFormatContext, NULL, NULL, outputFile);
    if (!outFormatContext) {
        cout << "\nerror in allocating av format output context";
        exit(1);
    }

    if (PrepareVideoEncoder()) {
        exit(1);
    }

    if (PrepareAudioEncoder()) {
        exit(1);
    }

    if (!outFormatContext->nb_streams) {
        cout << "\noutput file dose not contain any stream";
        exit(1);
    }

    /*
     * Some container formats (like MP4) require global headers to be present
     * Mark the encoder so that it behaves accordingly.
     */
    if (outFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        outFormatContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    /* create empty video file */
    if (!(outFormatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outFormatContext->pb, outputFile, AVIO_FLAG_WRITE) < 0) {
            cout << "\nerror in creating the video file";
            exit(1);
        }
    }

    /* imp: mp4 container or some advanced container file required header information */
    ret = avformat_write_header(outFormatContext, &videoOptions);
    if (ret < 0) {
        cout << "\nerror in writing the header context";
        exit(1);
    }

    /* uncomment here to view the complete video file informations */
    cout<<"\n\nOutput file information :\n\n";
    av_dump_format(outFormatContext , 0 ,outputFile ,1);

    return 0;
}

int ScreenRecorder::SelectArea() {
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
        offsetX = 0;
        offsetY = 0;
    } else {
        width = rw;
        height = rh;
        offsetX = rx;
        offsetY = ry;
    }

    XCloseDisplay(disp);

    cout << "\nSize: " << width << "x" << height << endl;
    cout << "\nOffset (x,y): " << offsetX << "," << offsetY << endl;

    return EXIT_SUCCESS;
}