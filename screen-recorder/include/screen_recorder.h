#pragma once

#include <common/common.h>
#include <format/demuxer.h>
#include <format/muxer.h>
#include <process/pipeline.h>

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "video_parameters.h"

class ScreenRecorder {
    /* Whether the recorder should be verbose or not */
    bool verbose_;

    /* Synchronization variables */

    bool stop_capture_;
    bool paused_;
    bool stopped_;
    std::mutex m_;
    std::condition_variable cv_;
    std::thread capturer_;
#ifdef LINUX
    std::thread audio_capturer_;
#endif

    /* The pipeline used for audio/video processing */
    std::unique_ptr<Pipeline> pipeline_;

    /* The muxer managing the output file */
    std::shared_ptr<Muxer> muxer_;

    /**
     * Capture packets from a demuxer and send them to the pipeline
     * @param demuxer the demuxer to read packets from
     */
    void capture(std::unique_ptr<Demuxer> demuxer);

public:
    /**
     * Create a new Screen Recorder
     * @param verbose true to make the recorder verbose, false to use the default verbosity
     */
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

    /**
     * Set the verbosity of the screen recorder
     * @param verbose true to make the recorder verbose, false to use the default verbosity
     */
    void setVerbose(bool verbose);

    /**
     * Print a list of the devices available for capturing
     */
    void listAvailableDevices() const;
};