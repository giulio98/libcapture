#include "../include/ScreenRecorder.h"

#ifdef LINUX
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

#include <stdio.h>
#include <stdlib.h>

using namespace std;

static int64_t pts = 0;

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

static void cleanupContext(AVFormatContext **avfc) {
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
static int init_packet(AVPacket **packet) {
    if (!(*packet = av_packet_alloc())) {
        fprintf(stderr, "Could not allocate packet\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

static int init_input_frame(AVFrame **frame) {
    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Could not allocate input frame\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

static int init_output_frame(AVFrame **frame, AVCodecContext *output_codec_context, int frame_size) {
    int error;

    /* Create a new frame to store the audio samples. */
    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Could not allocate output frame\n");
        return AVERROR_EXIT;
    }

    /* Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity. */
    (*frame)->nb_samples = frame_size;
    (*frame)->channel_layout = output_codec_context->channel_layout;
    (*frame)->format = output_codec_context->sample_fmt;
    (*frame)->sample_rate = output_codec_context->sample_rate;

    /* Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified. */
    if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
        fprintf(stderr, "Could not allocate output frame samples (error '%d')\n", error);
        av_frame_free(frame);
        return error;
    }

    return 0;
}

// static int decode_audio_frame(AVFrame *frame, AVFormatContext *input_format_context,
//                               AVCodecContext *input_codec_context, int *data_present, int *finished) {
int ScreenRecorder::decodeAudioFrame(AVPacket *inPacket, AVFrame *inFrame, int *data_present, int *finished) {
    /* Packet used for temporary storage. */
    int error;

    /* Read one audio frame from the input file into a temporary packet. */
    if ((error = av_read_frame(inAudioFormatContext, inPacket)) < 0) {
        /* If we are at the end of the file, flush the decoder below. */
        if (error == AVERROR_EOF)
            *finished = 1;
        else {
            fprintf(stderr, "Could not read frame (error '%d')\n", error);
            goto cleanup;
        }
    }

    /* Send the audio frame stored in the temporary packet to the decoder.
     * The input audio stream decoder is used to do this. */
    if ((error = avcodec_send_packet(inAudioCodecContext, inPacket)) < 0) {
        fprintf(stderr, "Could not send packet for decoding (error '%d')\n", error);
        goto cleanup;
    }

    /* Receive one frame from the decoder. */
    error = avcodec_receive_frame(inAudioCodecContext, inFrame);
    /* If the decoder asks for more data to be able to decode a frame,
     * return indicating that no data is present. */
    if (error == AVERROR(EAGAIN)) {
        error = 0;
        goto cleanup;
        /* If the end of the input file is reached, stop decoding. */
    } else if (error == AVERROR_EOF) {
        *finished = 1;
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not decode frame (error '%d')\n", error);
        goto cleanup;
        /* Default case: Return decoded data. */
    } else {
        *data_present = 1;
        goto cleanup;
    }

cleanup:
    // av_packet_free(&inPacket);
    return error;
}

static int convert_samples(const uint8_t **input_data, uint8_t **converted_data, const int frame_size,
                           SwrContext *resample_context) {
    int error;

    /* Convert the samples using the resampler. */
    if ((error = swr_convert(resample_context, converted_data, frame_size, input_data, frame_size)) < 0) {
        fprintf(stderr, "Could not convert input samples (error '%d')\n", error);
        return error;
    }

    return 0;
}

static int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_input_samples, const int frame_size) {
    int error;

    /* Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples. */
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
        fprintf(stderr, "Could not reallocate FIFO\n");
        return error;
    }

    /* Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void **)converted_input_samples, frame_size) < frame_size) {
        fprintf(stderr, "Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

static int init_converted_samples(uint8_t ***converted_input_samples, AVCodecContext *output_codec_context,
                                  int frame_size) {
    int error;

    /* Allocate as many pointers as there are audio channels.
     * Each pointer will later point to the audio samples of the corresponding
     * channels (although it may be NULL for interleaved formats).
     */
    if (!(*converted_input_samples =
              static_cast<uint8_t **>(calloc(output_codec_context->channels, sizeof(**converted_input_samples))))) {
        fprintf(stderr, "Could not allocate converted input sample pointers\n");
        return AVERROR(ENOMEM);
    }

    /* Allocate memory for the samples of all channels in one consecutive
     * block for convenience. */
    if ((error = av_samples_alloc(*converted_input_samples, NULL, output_codec_context->channels, frame_size,
                                  output_codec_context->sample_fmt, 0)) < 0) {
        fprintf(stderr, "Could not allocate converted input samples (error '%d')\n", error);
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return error;
    }
    return 0;
}

// static int read_decode_convert_and_store(AVAudioFifo *fifo, AVFormatContext *input_format_context,
//                                          AVCodecContext *input_codec_context, AVCodecContext *output_codec_context,
//                                          SwrContext *resampler_context, int *finished) {
int ScreenRecorder::readDecodeConvertStore(AVPacket *inPacket, int *finished) {
    /* Temporary storage of the input samples of the frame read from the file. */
    AVFrame *input_frame = NULL;
    /* Temporary storage for the converted input samples. */
    uint8_t **converted_input_samples = NULL;
    int data_present = 0;
    int ret = AVERROR_EXIT;

    /* Initialize temporary storage for one input frame. */
    if (init_input_frame(&input_frame)) goto cleanup;
    /* Decode one frame worth of audio samples. */
    // if (decode_audio_frame(input_frame, inAudioFormatContext, inAudioCodecContext, &data_present, finished))
    if (decodeAudioFrame(inPacket, input_frame, &data_present, finished)) goto cleanup;
    /* If we are at the end of the file and there are no more samples
     * in the decoder which are delayed, we are actually finished.
     * This must not be treated as an error. */
    if (*finished) {
        ret = 0;
        goto cleanup;
    }
    /* If there is decoded data, convert and store it. */
    if (data_present) {
        /* Initialize the temporary storage for the converted input samples. */
        if (init_converted_samples(&converted_input_samples, outAudioCodecContext, input_frame->nb_samples))
            goto cleanup;

        /* Convert the input samples to the desired output sample format.
         * This requires a temporary storage provided by converted_input_samples. */
        if (convert_samples((const uint8_t **)input_frame->extended_data, converted_input_samples,
                            input_frame->nb_samples, audioResampleContext))
            goto cleanup;

        /* Add the converted input samples to the FIFO buffer for later processing. */
        if (add_samples_to_fifo(audioFifoBuffer, converted_input_samples, input_frame->nb_samples)) goto cleanup;
        ret = 0;
    }
    ret = 0;

cleanup:
    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        free(converted_input_samples);
    }
    av_frame_free(&input_frame);

    return ret;
}

static int encode_audio_frame(AVFrame *frame, AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context, int *data_present) {
    /* Packet used for temporary storage. */
    AVPacket *output_packet;
    int error;

    error = init_packet(&output_packet);
    if (error < 0) return error;

    /* Set a timestamp based on the sample rate for the container. */
    if (frame) {
        frame->pts = pts;
        pts += frame->nb_samples;
    }

    /* Send the audio frame stored in the temporary packet to the encoder.
     * The output audio stream encoder is used to do this. */
    error = avcodec_send_frame(output_codec_context, frame);
    /* The encoder signals that it has nothing more to encode. */
    if (error == AVERROR_EOF) {
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not send packet for encoding (error '%d')\n", error);
        goto cleanup;
    }

    /* Receive one encoded frame from the encoder. */
    error = avcodec_receive_packet(output_codec_context, output_packet);
    /* If the encoder asks for more data to be able to provide an
     * encoded frame, return indicating that no data is present. */
    if (error == AVERROR(EAGAIN)) {
        error = 0;
        goto cleanup;
        /* If the last frame has been encoded, stop encoding. */
    } else if (error == AVERROR_EOF) {
        error = 0;
        goto cleanup;
    } else if (error < 0) {
        fprintf(stderr, "Could not encode frame (error '%d')\n", error);
        goto cleanup;
        /* Default case: Return encoded data. */
    } else {
        *data_present = 1;
    }

    // Very ugly, improve this!
    output_packet->stream_index = 1;

    /* Write one audio frame from the temporary packet to the output file. */
    if (*data_present && (error = av_write_frame(output_format_context, output_packet)) < 0) {
        fprintf(stderr, "Could not write frame (error '%d')\n", error);
        goto cleanup;
    }

cleanup:
    av_packet_free(&output_packet);
    return error;
}

static int load_encode_and_write(AVAudioFifo *fifo, AVFormatContext *output_format_context,
                                 AVCodecContext *output_codec_context) {
    /* Temporary storage of the output samples of the frame written to the file. */
    AVFrame *output_frame;
    /* Use the maximum number of possible samples per frame.
     * If there is less than the maximum possible frame size in the FIFO
     * buffer use this number. Otherwise, use the maximum possible frame size. */
    const int frame_size = FFMIN(av_audio_fifo_size(fifo), output_codec_context->frame_size);
    int data_written;

    /* Initialize temporary storage for one output frame. */
    if (init_output_frame(&output_frame, output_codec_context, frame_size)) return AVERROR_EXIT;

    /* Read as many samples from the FIFO buffer as required to fill the frame.
     * The samples are stored in the frame temporarily. */
    if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) < frame_size) {
        fprintf(stderr, "Could not read data from FIFO\n");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    /* Encode one frame worth of audio samples. */
    if (encode_audio_frame(output_frame, output_format_context, output_codec_context, &data_written)) {
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }
    av_frame_free(&output_frame);
    return 0;
}

static int write_output_file_trailer(AVFormatContext *output_format_context) {
    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        fprintf(stderr, "Could not write output file trailer (error '%d')\n", error);
        return error;
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

    outAudioCodecContext = avcodec_alloc_context3(outAudioCodec);
    if (!outAudioCodecContext) {
        cerr << "\nfailed to allocate output codec context";
        return 1;
    }

    int output_channels = 2;
    int bit_rate = 96000;

    /* set property of the video file */
    outAudioCodecContext->channels = output_channels;
    outAudioCodecContext->channel_layout = av_get_default_channel_layout(output_channels);
    outAudioCodecContext->sample_rate = inAudioCodecContext->sample_rate;
    outAudioCodecContext->sample_fmt = outAudioCodec->sample_fmts[0];
    outAudioCodecContext->bit_rate = bit_rate;
    outAudioCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    outAudioStream->time_base.den = inAudioCodecContext->sample_rate;
    outAudioStream->time_base.num = 1;

    ret = avcodec_open2(outAudioCodecContext, outAudioCodec, NULL);
    if (ret < 0) {
        cout << "\nerror in opening the avcodec";
        return 1;
    }

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
    cleanupContext(&inVideoFormatContext);
    cleanupContext(&inAudioFormatContext);
}

/* function to capture and store data in frames by allocating required memory and auto deallocating the memory.   */
int ScreenRecorder::CaptureVideoFrame(AVPacket *inPacket) {
    /*
     * When you decode a single packet, you still don't have information enough to have a frame
     * [depending on the type of codec, some of them you do], when you decode a GROUP of packets
     * that represents a frame, then you have a picture! that's why frameFinished
     * will let you know you decoded enough to have a frame.
     */
    // int frameFinished;
    int ret;

    /* Compressed (encoded) video data */
    // AVPacket *inPacket;
    /* Decoded video data (input) */
    AVFrame *inFrame;
    /* Decoded video data (output) */
    AVFrame *outFrame;

    // DEPRECATED:
    // packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    // av_init_packet(packet);
    //
    // inPacket = av_packet_alloc();
    // if (!inPacket) {
    //     cerr << "\nunable to allocate packet";
    //     exit(1);
    // }

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

    // int ii = 0;
    // int no_frames = 100;
    // cout << "\nenter No. of frames to capture : ";
    // cin >> no_frames;

    AVPacket *outPacket;
    int j = 0;

    // while (av_read_frame(inVideoFormatContext, inPacket) >= 0) {
        // if (ii++ == no_frames) break;
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
    // }  // End of while-loop

    // ret = av_write_trailer(outFormatContext);
    // if (ret < 0) {
    //     cout << "\nerror in writing av trailer";
    //     exit(1);
    // }

    // // THIS WAS ADDED LATER
    // av_free(video_outbuf);
    // avio_close(outFormatContext->pb);

    return 0;
}

int ScreenRecorder::PrepareAudioResampler() {
    int ret;

    /*
     * Create a resampler context for the conversion.
     * Set the conversion parameters.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity (they are sometimes not detected
     * properly by the demuxer and/or decoder).
     */
    audioResampleContext = swr_alloc_set_opts(
        NULL, av_get_default_channel_layout(outAudioCodecContext->channels), outAudioCodecContext->sample_fmt,
        outAudioCodecContext->sample_rate, av_get_default_channel_layout(inAudioCodecContext->channels),
        inAudioCodecContext->sample_fmt, inAudioCodecContext->sample_rate, 0, NULL);
    if (!audioResampleContext) {
        fprintf(stderr, "Could not allocate resample context\n");
        return AVERROR(ENOMEM);
    }
    /*
     * Perform a sanity check so that the number of converted samples is
     * not greater than the number of samples to be converted.
     * If the sample rates differ, this case has to be handled differently
     */
    // av_assert0(outAudioCodecContext->sample_rate == inAudioCodecContext->sample_rate);
    if (outAudioCodecContext->sample_rate != inAudioCodecContext->sample_rate) {
        cerr << "\nline 455: big error";
        return 1;
    }

    /* Open the resampler with the specified parameters. */
    if ((ret = swr_init(audioResampleContext)) < 0) {
        fprintf(stderr, "Could not open resample context\n");
        swr_free(&audioResampleContext);
        return ret;
    }

    return 0;
}

int ScreenRecorder::PrepareAudioFifo() {
    if (!(audioFifoBuffer = av_audio_fifo_alloc(outAudioCodecContext->sample_fmt, outAudioCodecContext->channels, 1))) {
        fprintf(stderr, "Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }

    return 0;
}

int ScreenRecorder::CaptureAudioFrame(AVPacket *inPacket) {
    int ret;
    int no_frames = 0;
    int max_frames = 100;

    // while (++no_frames < max_frames) {

        /* Use the encoder's desired frame size for processing. */
        const int output_frame_size = outAudioCodecContext->frame_size;
        int finished = 0;

        /* Make sure that there is one frame worth of samples in the FIFO
         * buffer so that the encoder can do its work.
         * Since the decoder's and the encoder's frame size may differ, we
         * need to FIFO buffer to store as many frames worth of input samples
         * that they make up at least one frame worth of output samples. */
        while (av_audio_fifo_size(audioFifoBuffer) < output_frame_size) {
            /* Decode one frame worth of audio samples, convert it to the
             * output sample format and put it into the FIFO buffer. */
            // if (read_decode_convert_and_store(audioFifoBuffer, inAudioFormatContext, inAudioCodecContext,
            //                                   outAudioCodecContext, audioResampleContext, &finished))
            //     goto cleanup;
            if (readDecodeConvertStore(inPacket, &finished)) goto cleanup;

            /* If we are at the end of the input file, we continue
             * encoding the remaining audio samples to the output file. */
            if (finished) break;
        }

        /* If we have enough samples for the encoder, we encode them.
         * At the end of the file, we pass the remaining samples to
         * the encoder. */
        while (av_audio_fifo_size(audioFifoBuffer) >= output_frame_size ||
               (finished && av_audio_fifo_size(audioFifoBuffer) > 0))
            /* Take one frame worth of audio samples from the FIFO buffer,
             * encode it and write it to the output file. */
            if (load_encode_and_write(audioFifoBuffer, outFormatContext, outAudioCodecContext)) goto cleanup;

        /* If we are at the end of the input file and have encoded
         * all remaining samples, we can exit this loop and finish. */
        if (finished) {
            int data_written;
            /* Flush the encoder as it may have delayed frames. */
            do {
                data_written = 0;
                if (encode_audio_frame(NULL, outFormatContext, outAudioCodecContext, &data_written)) goto cleanup;
            } while (data_written);
            // break;
        }
    // }

    /* Write the trailer of the output file container. */
    // if (write_output_file_trailer(outFormatContext)) goto cleanup;
    ret = 0;

cleanup:
    // if (audioFifoBuffer) av_audio_fifo_free(audioFifoBuffer);
    // swr_free(&audioResampleContext);
    // if (outAudioCodecContext) avcodec_free_context(&outAudioCodecContext);
    // if (outFormatContext) {
    //     avio_closep(&outFormatContext->pb);
    //     avformat_free_context(outFormatContext);
    // }
    // if (inAudioCodecContext) avcodec_free_context(&inAudioCodecContext);
    // if (inAudioFormatContext) avformat_close_input(&inAudioFormatContext);

    return ret;
}

int ScreenRecorder::CaptureFrames() {
    int ret;
    int no_frames = 0;
    int max_frames = 200;

    /* Compressed (encoded) video data */
    AVPacket *inVideoPacket;
    AVPacket *inAudioPacket;

    inVideoPacket = av_packet_alloc();
    if (!inVideoPacket) {
        cerr << "\nunable to allocate packet";
        exit(1);
    }

    inAudioPacket = av_packet_alloc();
    if (!inAudioPacket) {
        cerr << "\nunable to allocate packet";
        exit(1);
    }

    PrepareAudioResampler();
    PrepareAudioFifo();

    while (++no_frames < max_frames) {
        av_read_frame(inVideoFormatContext, inVideoPacket);
        av_read_frame(inAudioFormatContext, inAudioPacket);

        CaptureVideoFrame(inVideoPacket);
        CaptureAudioFrame(inAudioPacket);
    }

    ret = write_output_file_trailer(outFormatContext);
    if (ret) {
        cerr << "Error wriding output file trailer";
        return 1;
    }

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
    // ret = avformat_open_input(&inAudioFormatContext, "default", inAudioFormat, &audioOptions);
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
    // ret = avformat_write_header(outFormatContext, &videoOptions);
    ret = avformat_write_header(outFormatContext, NULL);
    if (ret < 0) {
        cout << "\nerror in writing the header context";
        exit(1);
    }

    /* uncomment here to view the complete video file informations */
    cout << "\n\nOutput file information :\n\n";
    av_dump_format(outFormatContext, 0, outputFile, 1);

    return 0;
}

#ifdef LINUX

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

#endif