#pragma once

#include <array>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "audio_converter.h"
#include "audio_encoder.h"
#include "common.h"
#include "decoder.h"
#include "demuxer.h"
#include "muxer.h"
#include "video_converter.h"
#include "video_encoder.h"

class ScreenRecorder {
    bool capture_audio_;

    /* Synchronization variables */

    bool stop_capture_;
    bool paused_;
    std::mutex m_;
    std::condition_variable status_cv_;
    std::thread recorder_thread_;  // thread responsible for recording video and audio

    /* Recording parameters */

    int video_offset_x_;
    int video_offset_y_;
    int video_width_;
    int video_height_;
    int video_framerate_;
    AVPixelFormat out_video_pix_fmt_;
    AVCodecID out_video_codec_id_;
    AVCodecID out_audio_codec_id_;
    std::string output_file_;

    /* Format and device names */

    std::string in_fmt_name_;
    std::string device_name_;
#ifdef LINUX
    std::string in_audio_fmt_name_;
    std::string audio_device_name_;
#endif

    /* Structures for audio-video processing */

    std::unique_ptr<Demuxer> demuxer_;
#ifdef LINUX
    std::unique_ptr<Demuxer> audio_demuxer_;
#endif
    std::unique_ptr<Muxer> muxer_;
    std::array<std::unique_ptr<Decoder>, av::DataType::NumDataTypes> decoders_;
    std::array<std::unique_ptr<Encoder>, av::DataType::NumDataTypes> encoders_;
    std::array<std::unique_ptr<Converter>, av::DataType::NumDataTypes> converters_;

    std::map<std::string, std::string> video_encoder_options_;
    std::map<std::string, std::string> audio_encoder_options_;

    std::array<std::deque<av::PacketUPtr>, av::DataType::NumDataTypes> packets_queues_;
    std::array<std::condition_variable, av::DataType::NumDataTypes> queues_cv_;

    /* Counters of video and audio frames used to compute PTSs */
    std::array<int64_t, av::DataType::NumDataTypes> frame_counters_;

    /* Recording start time */
    int64_t start_time_;
    /* Counter of times in which the estimated framerate is lower than the specified one */
    int dropped_frame_counter_;

    /**
     * Set the ScreenRecorder recording parameters after performing some basic validation.
     * WARNING: This function has to be called before initInput()
     */
    void setParams(const std::string &video_device, const std::string &audio_device, const std::string &output_file,
                   int video_width, int video_height, int video_offset_x, int video_offset_y, int framerate);

    /**
     * Adjust the ScreenRecorder video measures in case the whole display is recorder
     * and perform some basic validation.
     * WARNING: This function has to called after initInput() and before initOutput()
     */
    void adjustVideoSize();

    /**
     * Initialize the demuxer[s] and the decoders
     */
    void initInput();

    /**
     * Initialize the encoders and the muxer.
     * WARNING: This function must be called after initInput() and adjustVideoSize()
     */
    void initOutput();

    /**
     * Initialize the converters.
     * WARNING: This function must be called after both initInput() and initOutput()
     */
    void initConverters();

    /**
     * Start the actual video[and audio] recording
     * WARNING: This function must be called after initializing all the processing structures
     * using initInput() initOutput() and initConverters()
     */
    void capture();

    /**
     * Read the packets coming from the specified demuxer and store them in the corresponding queue
     * @param demuxer           an observer pointer to the demuxer to se to read the packets
     * @param handle_start_time whether the function should internally handle the start_time variable
     * when a pause is performed (WARNING: in case of multiple concurrent threads, only one of them should
     * handle the start_time)
     */
    void readPackets(Demuxer *demuxer, bool handle_start_time = false);

    /**
     * Process the packets stored in the queue corresponding to data_type
     * @param data_type the data type of the packets to process
     */
    void processPackets(av::DataType data_type);

    /**
     * Stop the capturing and notify all the CV
     */
    void stopAndNotify() noexcept;

    /**
     * Process an encoded packet (decode, convert, encode and write it)
     * @param packet    an observer pointer to the packet to process
     * @param data_type the type of data contained on the packet (audio or video)
     */
    void processPacket(const AVPacket *packet, av::DataType data_type);

    /**
     * Process a converted frame (encode and write it)
     * @param frame     an observer pointer to the frame to process
     * @param data_type the type of data contained on the frame (audio or video)
     */
    void processConvertedFrame(const AVFrame *frame, av::DataType data_type);

    /**
     * Flush the processing structures (decoders, encoders and muxer),
     * removing all the packets/frames still contained in the internal queues
     * WARNING: After calling this function, it won't be possible to use the processing structures anymore
     */
    void flushPipelines();

    /**
     * Estimate the actual recording framerate and print to stderr in case of supposed frame drops
     */
    void estimateFramerate();

    /**
     * Print information about the input and output streams
     * WARNING: This function must be called after initInput() and initOutput()
     */
    void printInfo() const;

#ifdef WINDOWS
    void setDisplayResolution() const;
#endif

public:
    ScreenRecorder();

    ~ScreenRecorder();

    /**
     * Start the video[and audio] recording
     * @param video_device      the name of the video device to use
     * @param audio_device      the name of the audio device to use (if empty, audio won't be recorded)
     * @param output_file       the name of the output file to use to save the recording
     * @param video_width       the width of the video (if 0, the display width will be used)
     * @param video_height      the height of the video (if 0, the display height will be used)
     * @param video_offset_x    the horizontal offset of the captured portion of the display
     * @param video_offset_y    the vertical offset of the captured portion of the display
     * @param framerate         the video framerate to use
     */
    void start(const std::string &video_device, const std::string &audio_device, const std::string &output_file,
               int video_width, int video_height, int video_offset_x, int video_offset_y, int framerate);

    /**
     * Stop the screen capture
     */
    void stop();

    /**
     * Pause the screen capture.
     * If the recording is already pause/stopped, nothing will be done
     */
    void pause();

    /**
     * Resume the screen capture
     * If the recording is already proceeding/stopped, nothing will be done
     */
    void resume();

    /**
     * Print a list of the devices available for capturing
     */
    void listAvailableDevices();

#ifdef WINDOWS
    static std::vector<std::string> getInputAudioDevices();
#endif
};