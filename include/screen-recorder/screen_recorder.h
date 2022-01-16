#pragma once

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "video_parameters.h"

class Demuxer;
class Muxer;
class Pipeline;

class ScreenRecorder {
    /* Whether the recorder should be verbose or not */
    bool verbose_;

    /* Synchronization variables */

    bool stopped_ = true;
    bool paused_;
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
    void capture(Demuxer demuxer);

    /**
     * Stop and join the capturer threads
     */
    void stopCapturers();

public:
    /**
     * Create a new Screen Recorder
     * @param verbose true to make the recorder verbose, false to use the default verbosity
     */
    explicit ScreenRecorder(bool verbose = false);

    ScreenRecorder(const ScreenRecorder &) = delete;

    ~ScreenRecorder();

    ScreenRecorder &operator=(const ScreenRecorder &) = delete;

    /**
     * Start the video [and audio] recording.
     * When this function returns, the recorder internal state will be fully initialized
     * and the recording will be in progress.
     * The video stream will be encoded as H.264, with pixel format YUV420, while the audio stream will
     * be encoded as AAC.
     * WARNING: if there is already a recording in progress, calling this function will throw an exception
     * @param video_device      the name of the video device to use
     * @param audio_device      the name of the audio device to use (if empty, audio won't be recorded)
     * @param output_file       the name of the output file to use to save the recording
     * @param video_params      the video dimensions (NOTE: if width/height is set to 0, the whole display will be
     * considered)
     */
    void start(const std::string &video_device, const std::string &audio_device, const std::string &output_file,
               VideoParameters video_params);

    /**
     * Stop the recording.
     * When this function returns, the recorder internal state will have been complemetely cleaned up
     * (including all processing threads)
     */
    void stop();

    /**
     * Pause the screen capture.
     * If the recording is already paused/stopped, nothing will be done
     */
    void pause();

    /**
     * Resume the screen capture.
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