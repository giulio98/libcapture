#pragma once

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "common.h"
#include "demuxer.h"
#include "muxer.h"
#include "pipeline.h"
#include "video_dimensions.h"

class ScreenRecorder {
    /* Synchronization variables */

    bool stop_capture_;
    bool paused_;
    bool stopped_;
    std::mutex m_;
    std::condition_variable status_cv_;
    std::thread recorder_thread_;  // thread responsible for recording video and audio
#ifdef LINUX
    std::thread audio_recorder_thread_;
#endif

    /* Recording parameters */

    bool verbose_;
    AVPixelFormat out_video_pix_fmt_;
    AVCodecID out_video_codec_id_;
    AVCodecID out_audio_codec_id_;
    std::string in_fmt_name_;
#ifdef LINUX
    std::string in_audio_fmt_name_;
#endif

    /* Structures for audio-video processing */

    std::shared_ptr<Muxer> muxer_;
    std::shared_ptr<Demuxer> demuxer_;
    std::unique_ptr<Pipeline> pipeline_;
#ifdef LINUX
    std::shared_ptr<Demuxer> audio_demuxer_;
    std::unique_ptr<Pipeline> audio_pipeline_;
#endif

    void capture(Demuxer *demuxer, Pipeline *pipeline);

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
     * @param video_dims        the video dimensions (NOTE: if width/height is set to 0, the whole display will be
     * considered)
     * @param framerate         the video framerate to use
     * @param verbose           the level of verboseness during the recording
     */
    void start(const std::string &video_device, const std::string &audio_device, const std::string &output_file,
               VideoDimensions video_dims, int framerate, bool verbose = false);

    /**
     * Stop the screen capture
     */
    void stop();

    /**
     * Pause the screen capture.
     * If the recording is already paused/stopped, nothing will be done
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
};