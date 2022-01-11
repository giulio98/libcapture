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
#include "video_parameters.h"

class ScreenRecorder {
    /* Synchronization variables */

    bool stop_capture_;
    bool paused_;
    bool stopped_;
    std::mutex m_;
    std::condition_variable cv_;
    std::thread recorder_thread_;  // thread responsible for recording video and audio
#ifdef LINUX
    std::thread audio_recorder_thread_;
#endif

    /* Recording parameters */

    bool verbose_;

    /* Structures for audio-video processing */

    std::shared_ptr<Muxer> muxer_;

    void capture(std::unique_ptr<Pipeline> pipeline);

public:
    ScreenRecorder(bool verbose = false);

    ~ScreenRecorder();

    /**
     * Start the video[and audio] recording
     * @param video_device      the name of the video device to use
     * @param audio_device      the name of the audio device to use (if empty, audio won't be recorded)
     * @param output_file       the name of the output file to use to save the recording
     * @param video_params        the video dimensions (NOTE: if width/height is set to 0, the whole display will be
     * considered)
     */
    void start(const std::string &video_device, const std::string &audio_device, const std::string &output_file,
               VideoParameters video_params);

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

    void setVerbose(bool verbose);

    /**
     * Print a list of the devices available for capturing
     */
    void listAvailableDevices() const;
};