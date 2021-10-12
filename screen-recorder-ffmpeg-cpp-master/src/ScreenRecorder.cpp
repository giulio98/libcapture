#include "../include/ScreenRecorder.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>

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

/* initialize the resources*/
ScreenRecorder::ScreenRecorder() {
    // av_register_all();
    // avcodec_register_all();
    avdevice_register_all();
    cout << "\nall required functions are registered successfully";
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder() {
    avformat_close_input(&inFormatContext);
    if (!inFormatContext) {
        cout << "\nfile closed sucessfully";
    } else {
        cout << "\nunable to close the file";
        exit(1);
    }

    avformat_free_context(inFormatContext);
    if (!inFormatContext) {
        cout << "\navformat free successfully";
    } else {
        cout << "\nunable to free avformat context";
        exit(1);
    }
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

    int nbytes =
        av_image_get_buffer_size(outCodecContext->pix_fmt, outCodecContext->width, outCodecContext->height, 32);
    uint8_t *video_outbuf = (uint8_t *)av_malloc(nbytes);
    if (video_outbuf == NULL) {
        cout << "\nunable to allocate memory";
        exit(1);
    }

    // Setup the data pointers and linesizes based on the specified image parameters and the provided array.
    ret = av_image_fill_arrays(outFrame->data, outFrame->linesize, video_outbuf, AV_PIX_FMT_YUV420P,
                               outCodecContext->width, outCodecContext->height,
                               1);  // returns : the size in bytes required for src
    if (ret < 0) {
        cout << "\nerror in filling image array";
    }

    SwsContext *swsCtx_;

    // Allocate and return swsContext.
    // a pointer to an allocated context, or NULL in case of error
    // Deprecated : Use sws_getCachedContext() instead.
    swsCtx_ = sws_getContext(videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
                             outCodecContext->width, outCodecContext->height, outCodecContext->pix_fmt, SWS_BICUBIC,
                             NULL, NULL, NULL);

    int ii = 0;
    int no_frames = 100;
    cout << "\nenter No. of frames to capture : ";
    cin >> no_frames;

    AVPacket outPacket;
    int j = 0;

    while (av_read_frame(inFormatContext, packet) >= 0) {
        if (ii++ == no_frames) break;
        if (packet->stream_index == videoStreamIdx) {
            ret = avcodec_send_packet(videoCodecContext, packet);
            if (ret < 0) {
                fprintf(stderr, "Error sending a packet for decoding\n");
                exit(1);
            }

            while (true) {
                ret = avcodec_receive_frame(videoCodecContext, inFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error during decoding\n");
                    exit(1);
                }

                // Convert the image from input (set in OpenDevices) to output format (set in InitOutputFile)
                sws_scale(swsCtx_, inFrame->data, inFrame->linesize, 0, videoCodecContext->height, outFrame->data,
                          outFrame->linesize);
                av_init_packet(&outPacket);
                outPacket.data = NULL;  // packet data will be allocated by the encoder
                outPacket.size = 0;

                outFrame->format = outCodecContext->pix_fmt;
                outFrame->width = outCodecContext->width;
                outFrame->height = outCodecContext->height;

                // we send a frame to the encoder
                ret = avcodec_send_frame(outCodecContext, outFrame);
                if (ret < 0) {
                    fprintf(stderr, "Error sending a frame for encoding\n");
                    exit(1);
                }

                while (ret >= 0) {
                    ret = avcodec_receive_packet(outCodecContext, &outPacket);
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
                            outPacket.pts =
                                av_rescale_q(outPacket.pts, outCodecContext->time_base, outVideoStream->time_base);
                        if (outPacket.dts != AV_NOPTS_VALUE)
                            outPacket.dts =
                                av_rescale_q(outPacket.dts, outCodecContext->time_base, outVideoStream->time_base);

                        printf("Write frame %3d (size= %2d)\n", j++, outPacket.size / 1000);
                        if (av_write_frame(outFormatContext, &outPacket) != 0) {
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

/* establishing the connection between camera or screen through its respective folder */
int ScreenRecorder::OpenDevices() {
    int ret;
    char str[20];
    AVInputFormat *inVideoFormat;
    AVInputFormat *inAudioFormat;
    AVCodecParameters *videoCodecParams;
    AVCodecParameters *audioCodecParams;

    inFormatContext = avformat_alloc_context();  // Allocate an AVFormatContext.

    /*
     * X11 video input device.
     * To enable this input device during configuration you need libxcb installed on your system. It will be
     * automatically detected during configuration. This device allows one to capture a region of an X11 display. refer
     * : https://www.ffmpeg.org/ffmpeg-devices.html#x11grab Current below is for screen recording. to connect with
     * camera use v4l2 as a input parameter for av_find_input_format
     */
    inVideoFormat = av_find_input_format("x11grab");
    inAudioFormat = av_find_input_format("alsa");

    /* Set the dictionary */
    videoOptions = NULL;
    video_dict_set(&videoOptions, "30", "1", width, height);

    sprintf(str, ":1.0+%d,%d", offsetX, offsetY);
    ret = avformat_open_input(&inFormatContext, str, inVideoFormat, &videoOptions);
    if (ret != 0) {
        cout << "\nerror in opening input video device";
        exit(1);
    }

    ret = avformat_open_input(&inFormatContext, "hw:0,0", inAudioFormat, NULL);
    if (ret != 0) {
        cout << "\nerror in opening input audio device";
        exit(1);
    }

    ret = avformat_find_stream_info(inFormatContext, NULL);
    if (ret < 0) {
        cout << "\nunable to find the stream information";
        exit(1);
    }

    videoStreamIdx = -1;
    audioStreamIdx = -1;

    /* find the first video stream index . Also there is an API available to do the below operations */
    for (int i = 0; i < inFormatContext->nb_streams; i++) {
        if (inFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) videoStreamIdx = i;
        if (inFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) audioStreamIdx = i;
        if (videoStreamIdx != -1 && audioStreamIdx != -1) break;
    }

    if (videoStreamIdx == -1) {
        cout << "\nunable to find the video stream index. (-1)";
        exit(1);
    }

    if (audioStreamIdx == -1) {
        cout << "\nunable to find the audio stream index. (-1)";
        exit(1);
    }

    videoCodecParams = inFormatContext->streams[videoStreamIdx]->codecpar;
    audioCodecParams = inFormatContext->streams[audioStreamIdx]->codecpar;

    // find decoder for the codec
    videoCodec = avcodec_find_decoder(videoCodecParams->codec_id);
    if (videoCodec == NULL) {
        cout << "\nunable to find the decoder for video";
        exit(1);
    }

    audioCodec = avcodec_find_decoder(audioCodecParams->codec_id);
    if (audioCodec == NULL) {
        cout << "\nunable to find the decoder for audio";
        exit(1);
    }

    videoCodecContext = avcodec_alloc_context3(videoCodec);
    if (!videoCodecContext) {
        cout << "\nfailed to allocated memory for videoCodecContext";
        return -1;
    }

    audioCodecContext = avcodec_alloc_context3(audioCodec);
    if (!audioCodecContext) {
        cout << "\nfailed to allocated memory for audioCodecContext";
        return -1;
    }

    // Fill the codec context based on the values from the supplied codec parameters
    // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
    if (avcodec_parameters_to_context(videoCodecContext, videoCodecParams) < 0) {
        cout << "\nfailed to copy codec params to codec context";
        return -1;
    }

    if (avcodec_parameters_to_context(audioCodecContext, audioCodecParams) < 0) {
        cout << "\nfailed to copy codec params to codec context";
        return -1;
    }

    // once we filled the codec context, we need to open the codec
    ret = avcodec_open2(videoCodecContext, videoCodec, NULL);
    if (ret < 0) {
        cout << "\nunable to open the av codec";
        exit(1);
    }

    ret = avcodec_open2(audioCodecContext, audioCodec, NULL);
    if (ret < 0) {
        cout << "\nunable to open the av codec";
        exit(1);
    }

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

    outVideoStream = avformat_new_stream(outFormatContext, NULL);
    if (!outVideoStream) {
        cout << "\nerror in creating a av format new stream";
        exit(1);
    }

    if (!outFormatContext->nb_streams) {
        cout << "\noutput file dose not contain any stream";
        exit(1);
    }

    outCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!outCodec) {
        cout << "\nerror in finding the av codecs. try again with correct codec";
        exit(1);
    }

    /* set property of the video file */
    outCodecContext = avcodec_alloc_context3(outCodec);
    outCodecContext->codec_id = AV_CODEC_ID_MPEG4;  // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
    outCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    outCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    outCodecContext->bit_rate = 400000;  // 2500000
    outCodecContext->width = width;
    outCodecContext->height = height;
    outCodecContext->gop_size = 3;
    outCodecContext->max_b_frames = 2;
    outCodecContext->time_base.num = 1;
    outCodecContext->time_base.den = 30;

    if (codecId == AV_CODEC_ID_H264) {
        av_opt_set(outCodecContext->priv_data, "preset", "slow", 0);
    }

    /*
     * Some container formats (like MP4) require global headers to be present
     * Mark the encoder so that it behaves accordingly.
     */
    if (outFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        outCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(outCodecContext, outCodec, NULL);
    if (ret < 0) {
        cout << "\nerror in opening the avcodec";
        exit(1);
    }

    ret = avcodec_parameters_from_context(outVideoStream->codecpar, outCodecContext);
    if (ret < 0) {
        cout << "\nerror in writing video stream parameters";
        exit(1);
    }
    outVideoStream->time_base = outCodecContext->time_base;

    /* create empty video file */
    if (!(outFormatContext->flags & AVFMT_NOFILE)) {
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
    // cout<<"\n\nOutput file information :\n\n";
    // av_dump_format(outFormatContext , 0 ,outputFile ,1);

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