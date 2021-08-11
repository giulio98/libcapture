#include "../include/ScreenRecorder.h"

using namespace std;

/* initialize the resources*/
ScreenRecorder::ScreenRecorder()
{

	// av_register_all();
	// avcodec_register_all();
	avdevice_register_all();
	cout<<"\nall required functions are registered successfully";
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder()
{

	avformat_close_input(&pAVFormatContext);
	if( !pAVFormatContext )
	{
		cout<<"\nfile closed sucessfully";
	}
	else
	{
		cout<<"\nunable to close the file";
		exit(1);
	}

	avformat_free_context(pAVFormatContext);
	if( !pAVFormatContext )
	{
		cout<<"\navformat free successfully";
	}
	else
	{
		cout<<"\nunable to free avformat context";
		exit(1);
	}

}

/* function to capture and store data in frames by allocating required memory and auto deallocating the memory.   */
int ScreenRecorder::CaptureVideoFrames()
{
	int flag;
	int frameFinished;//when you decode a single packet, you still don't have information enough to have a frame [depending on the type of codec, some of them //you do], when you decode a GROUP of packets that represents a frame, then you have a picture! that's why frameFinished will let //you know you decoded enough to have a frame.

	int frame_index = 0;
	value = 0;

	pAVPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(pAVPacket);

	pAVFrame = av_frame_alloc();
	if( !pAVFrame )
	{
	 cout<<"\nunable to release the avframe resources";
	 exit(1);
	}
	
	/*
	 * Since we're planning to output PPM files, which are stored in 24-bit RGB,
	 * we're going to have to convert our frame from its native format to RGB.
	 * ffmpeg will do these conversions for us. For most projects (including ours)
	 * we're going to want to convert our initial frame to a specific format.
	 * Let's allocate a frame for the converted frame now.
	 */
	outFrame = av_frame_alloc();//Allocate an AVFrame and set its fields to default values.
	if( !outFrame )
	{
	 cout<<"\nunable to release the avframe resources for outframe";
	 exit(1);
	}

	int video_outbuf_size;
	int nbytes = av_image_get_buffer_size(outAVCodecContext->pix_fmt,outAVCodecContext->width,outAVCodecContext->height,32);
	uint8_t *video_outbuf = (uint8_t*)av_malloc(nbytes);
	if( video_outbuf == NULL )
	{
		cout<<"\nunable to allocate memory";
		exit(1);
	}

	// Setup the data pointers and linesizes based on the specified image parameters and the provided array.
	value = av_image_fill_arrays( outFrame->data, outFrame->linesize, video_outbuf , AV_PIX_FMT_YUV420P, outAVCodecContext->width,outAVCodecContext->height,1 ); // returns : the size in bytes required for src
	if(value < 0)
	{
		cout<<"\nerror in filling image array";
	}

	SwsContext* swsCtx_ ;

	// Allocate and return swsContext.
	// a pointer to an allocated context, or NULL in case of error
	// Deprecated : Use sws_getCachedContext() instead.
	swsCtx_ = sws_getContext(pAVCodecContext->width,
		                pAVCodecContext->height,
		                pAVCodecContext->pix_fmt,
		                outAVCodecContext->width,
				outAVCodecContext->height,
		                outAVCodecContext->pix_fmt,
		                SWS_BICUBIC, NULL, NULL, NULL);


int ii = 0;
int no_frames = 100;
cout<<"\nenter No. of frames to capture : ";
cin>>no_frames;

	AVPacket outPacket;
	int j = 0;

	int got_picture;

	while( av_read_frame( pAVFormatContext , pAVPacket ) >= 0 )
	{
	if( ii++ == no_frames )break;
		if(pAVPacket->stream_index == VideoStreamIndx)
		{
			// value = avcodec_decode_video2( pAVCodecContext , pAVFrame , &frameFinished , pAVPacket );
			
			value = avcodec_send_packet(pAVCodecContext, pAVPacket);
    		if (value < 0) {
    		    fprintf(stderr, "Error sending a packet for decoding\n");
    		    exit(1);
    		}

			/*
			 * We only call 'receive_frame' once because we're working with videos
			 * and so we have not a single packet spanning multiple frames
			 */
    		value = avcodec_receive_frame(pAVCodecContext, pAVFrame);
			if (value == 0)
				frameFinished = 1;
    		else if (value == AVERROR(EAGAIN) || value == AVERROR_EOF)
    		    frameFinished = 0;
    		else if (value < 0) {
    		    fprintf(stderr, "Error during decoding\n");
    		    exit(1);
    		}

			if(frameFinished)// Frame successfully decoded :)
			{
				// Convert the image from input (set in openCamera) to output format (set in init_outputfile)
				sws_scale(swsCtx_, pAVFrame->data, pAVFrame->linesize,0, pAVCodecContext->height, outFrame->data,outFrame->linesize);
				av_init_packet(&outPacket);
				outPacket.data = NULL;    // packet data will be allocated by the encoder
				outPacket.size = 0;

				// avcodec_encode_video2(outAVCodecContext , &outPacket ,outFrame , &got_picture);
				
				outFrame->format = outAVCodecContext->pix_fmt;
				outFrame->width = outAVCodecContext->width;
				outFrame->height = outAVCodecContext->height;

				// we send a fram to the encoder
				value = avcodec_send_frame(outAVCodecContext, outFrame);
				if (value < 0) {
    				fprintf(stderr, "Error sending a frame for encoding\n");
    				exit(1);
				}
				while (value >= 0) {
    				value = avcodec_receive_packet(outAVCodecContext, &outPacket);
					if (value == AVERROR(EAGAIN)) {
						break;
					} else if (value == AVERROR_EOF) {
        				return 0;
     				}
    				else if (value < 0) {
        				fprintf(stderr, "Error during encoding\n");
        				exit(1);
    				}
    				// value = write_frame(outAVFormatContext, &outAVCodecContext->time_base, ost->st, &outPacket);
    				// if (value < 0) {
       				// fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(value));
       				// exit(1);
    				// }
    				//av_packet_unref(&outPacket);

					if(outPacket.size>0)
					{
						if(outPacket.pts != AV_NOPTS_VALUE)
							outPacket.pts = av_rescale_q(outPacket.pts, outAVCodecContext->time_base, video_st->time_base);
						if(outPacket.dts != AV_NOPTS_VALUE)
							outPacket.dts = av_rescale_q(outPacket.dts, outAVCodecContext->time_base, video_st->time_base);

						printf("Write frame %3d (size= %2d)\n", j++, outPacket.size/1000);
						if(av_write_frame(outAVFormatContext , &outPacket) != 0)
						{
							cout<<"\nerror in writing video frame";
						}

						av_packet_unref(&outPacket);
					} // got_picture

				}
				// return (frame) ? 0 : 1;

			// TO-DO: check if this is required	
			av_packet_unref(&outPacket);
			} // frameFinished

		}
	}// End of while-loop

	value = av_write_trailer(outAVFormatContext);
	if( value < 0)
	{
		cout<<"\nerror in writing av trailer";
		exit(1);
	}


//THIS WAS ADDED LATER
av_free(video_outbuf);
avio_close(outAVFormatContext->pb);
}

/* establishing the connection between camera or screen through its respective folder */
int ScreenRecorder::openCamera()
{

	value = 0;
	options = NULL;
	pAVFormatContext = NULL;

	pAVFormatContext = avformat_alloc_context();//Allocate an AVFormatContext.
/*

X11 video input device.
To enable this input device during configuration you need libxcb installed on your system. It will be automatically detected during configuration.
This device allows one to capture a region of an X11 display. 
refer : https://www.ffmpeg.org/ffmpeg-devices.html#x11grab
*/
	/* current below is for screen recording. to connect with camera use v4l2 as a input parameter for av_find_input_format */ 
	pAVInputFormat =const_cast<AVInputFormat*>( av_find_input_format("x11grab"));
	//viene generato uno stream di pacchetti
	/*
	Apriamo il file e leggiamo il suo header e rimpiamo AVFormatContext con le informazioni sul formato.
	se viene passato NULL in "AVInputFormat" ffmpeg indovina il formato
	per quanto riguarda linux il formato di input è "x11grab" permette la cattura di una regione di un x11 display"
	*/
  	value = avformat_open_input(&pAVFormatContext, ":0.0+10,250", pAVInputFormat, NULL);
	if(value != 0)
	{
	   cout<<"\nerror in opening input device";
	   exit(1);
	}

	/* set frame per second */
	/*
	settiamo il dizionario che sono le opzioni del demuxer
	*/
	value = av_dict_set( &options,"framerate","30",0 );
	if(value < 0)
	{
	  cout<<"\nerror in setting dictionary value";
	   exit(1);
	}

	value = av_dict_set( &options, "preset", "medium", 0 );
	if(value < 0)
	{
	  cout<<"\nerror in setting preset values";
	  exit(1);
	}

//	value = avformat_find_stream_info(pAVFormatContext,NULL);
	if(value < 0)
	{
	  cout<<"\nunable to find the stream information";
	  exit(1);
	}

	VideoStreamIndx = -1;

	/* find the first video stream index . Also there is an API available to do the below operations */
	for(int i = 0; i < pAVFormatContext->nb_streams; i++ ) // find video stream posistion/index.
	{
	  if( pAVFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO )
	  {
	     VideoStreamIndx = i;
	     break;
	  }

	} 

	if( VideoStreamIndx == -1)
	{
	  cout<<"\nunable to find the video stream index. (-1)";
	  exit(1);
	}

	// assign pAVFormatParameters to VideoStreamIndx
	pAVCodecParameters = pAVFormatContext->streams[VideoStreamIndx]->codecpar;
	//find decoder for the codec
	pAVCodec = const_cast<AVCodec*>(avcodec_find_decoder(pAVCodecParameters->codec_id));
	if( pAVCodec == NULL )
	{
	  cout<<"\nunable to find the decoder";
	  exit(1);
	}

	pAVCodecContext = avcodec_alloc_context3(pAVCodec);
  	if (!pAVCodecContext)
  	{
  		cout<<"\nfailed to allocated memory for AVCodecContext";
  		return -1;
  	}

  	// Fill the codec context based on the values from the supplied codec parameters
  	// https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
  	if (avcodec_parameters_to_context(pAVCodecContext, pAVCodecParameters) < 0)
  	{
  		cout<<"\nfailed to copy codec params to codec context";
  		return -1;
  	}

	//once we filled the codec context, we need to open the codec
	value = avcodec_open2(pAVCodecContext , pAVCodec , NULL);//Initialize the AVCodecContext to use the given AVCodec.
	if( value < 0 )
	{
	  cout<<"\nunable to open the av codec";
	  exit(1);
	}
}

/* initialize the video output file and its properties  */
int ScreenRecorder::init_outputfile()
{
	outAVFormatContext = NULL;
	value = 0;
	output_file = "./media/output.mp4";
	// options=NULL;

	avformat_alloc_output_context2(&outAVFormatContext, NULL, NULL, output_file);
	if (!outAVFormatContext)
	{
		cout<<"\nerror in allocating av format output context";
		exit(1);
	}

/* Returns the output format in the list of registered output formats which best matches the provided parameters, or returns NULL if there is no match. */
	/*output_format = const_cast<AVOutputFormat*>(av_guess_format(NULL, output_file ,NULL));
	if( !output_format )
	{
	 cout<<"\nerror in guessing the video format. try with correct format";
	 exit(1);
	}*/

	// int number_of_streams = pAVFormatContext->nb_streams;
	// int *streams_list = (int*)av_mallocz_array(number_of_streams, sizeof(*streams_list));
	// if(!streams_list) {
	// 	cout << "Error in allocating streams_list";
	// 	exit(1);
	// }

	// for (int i = 0; i < pAVFormatContext->nb_streams; i++) {
	// 	AVStream *out_stream;
    // 	AVStream *in_stream = pAVFormatContext->streams[i];
	// 	AVCodecParameters *in_codecpar = in_stream->codecpar;
    // 	if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
    // 	    in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
    // 	    in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
    // 	  streams_list[i] = -1;
    // 	  continue;
    // }
	// }

	

	// value = avcodec_parameters_copy(video_st->codecpar, pAVFormatContext->streams[VideoStreamIndx]->codecpar);
	// if (value < 0) {
	// 	cout << "\nerror copying parameters";
	// 	exit(1);
	// }

	av_dump_format(outAVFormatContext, 0, output_file, 1);

	/* create empty video file */
	if ( !(outAVFormatContext->flags & AVFMT_NOFILE) )
	{
	 if( avio_open2(&outAVFormatContext->pb , output_file , AVIO_FLAG_WRITE ,NULL, NULL) < 0 )
	 {
	  cout<<"\nerror in creating the video file";
	  exit(1);
	 }
	}
	video_st = avformat_new_stream(outAVFormatContext ,NULL);
	if( !video_st )
	{
		cout<<"\nerror in creating a av format new stream";
		exit(1);
	}
	video_st->codecpar->codec_id = AV_CODEC_ID_MPEG4;// AV_CODEC_ID_MPEG4; // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
	video_st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	// video_st->codecpar->pix_fmt  = AV_PIX_FMT_YUV420P;
	video_st->codecpar->bit_rate = 400000; // 2500000
	video_st->codecpar->width = 1920;
	video_st->codecpar->height = 1080;
	// video_st->codecpar->gop_size = 3;
	// video_st->codecpar->max_b_frames = 2;
	// video_st->codecpar->time_base.num = 1;
	// video_st->codecpar->time_base.den = 30; // 15fps

	/* TO-DO: remove */
	// outAVCodecContext = avcodec_alloc_context3(outAVCodec);
	// if( !outAVCodecContext )
	// {
	//   	cout<<"\nerror in allocating the codec contexts";
	// 	exit(1);
	// }
	outAVCodec = const_cast<AVCodec*>(avcodec_find_encoder(AV_CODEC_ID_MPEG4));
	if( !outAVCodec )
	{
	 cout<<"\nerror in finding the av codecs. try again with correct codec";
	exit(1);
	}
	/* set property of the video file */
	outAVCodecContext = avcodec_alloc_context3(outAVCodec);
	outAVCodecContext->codec_id = AV_CODEC_ID_MPEG4;// AV_CODEC_ID_MPEG4; // AV_CODEC_ID_H264 // AV_CODEC_ID_MPEG1VIDEO
	outAVCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	outAVCodecContext->pix_fmt  = AV_PIX_FMT_YUV420P;
	outAVCodecContext->bit_rate = 400000; // 2500000
	outAVCodecContext->width = 1920;
	outAVCodecContext->height = 1080;
	outAVCodecContext->gop_size = 3;
	outAVCodecContext->max_b_frames = 2;
	outAVCodecContext->time_base.num = 1;
	outAVCodecContext->time_base.den = 30; // 15fps

	if (codec_id == AV_CODEC_ID_H264)
	{
	 av_opt_set(outAVCodecContext->priv_data, "preset", "slow", 0);
	}

	/* Some container formats (like MP4) require global headers to be present
	   Mark the encoder so that it behaves accordingly. */

	if ( outAVFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		outAVCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	value = avcodec_open2(outAVCodecContext, outAVCodec, NULL);
	if( value < 0)
	{
		cout<<"\nerror in opening the avcodec";
		exit(1);
	}

	if(!outAVFormatContext->nb_streams)
	{
		cout<<"\noutput file dose not contain any stream";
	  	exit(1);
	}

	/* imp: mp4 container or some advanced container file required header information*/
	value = avformat_write_header(outAVFormatContext , &options);
	if(value < 0)
	{
		cout<<"\nerror in writing the header context";
		exit(1);
	}

	/*
	// uncomment here to view the complete video file informations
	cout<<"\n\nOutput file information :\n\n";
	av_dump_format(outAVFormatContext , 0 ,output_file ,1);
	*/
}


