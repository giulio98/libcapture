#pragma once

#include <array>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "video_parameters.h"

class Demuxer;
class Pipeline;

class Capturer {
    /* Whether the recorder should be verbose or not */
    bool verbose_;

    /* Synchronization variables */

    bool stopped_ = true;
    bool paused_{};
    std::mutex m_;
    std::condition_variable cv_;
    std::thread capturer_;

    /* The pipeline used for audio/video processing */
    std::unique_ptr<Pipeline> pipeline_;

    /**
     * Read packets from a demuxer and pass them to the processing pipeline
     * @param demuxer the demuxer to read the packets from
     */
    void capture(Demuxer &demuxer);

    /**
     * Read packets from two separate audio/video demuxers and pass them to the processing pipeline
     * @param video_demuxer the video demuxer to read packets from
     * @param audio_demuxer the audio demuxer to read packets from
     */
    void capture(Demuxer &video_demuxer, Demuxer &audio_demuxer);

    /**
     * Stop and join the capturer thread
     */
    void stopCapture();

public:
    /**
     * Create a new Screen Recorder
     * @param verbose true to make the recorder verbose, false to use the default verbosity
     */
    explicit Capturer(bool verbose = false);

    Capturer(const Capturer &) = delete;

    ~Capturer();

    Capturer &operator=(const Capturer &) = delete;

    /**
     * Start the video [and audio] recording.
     * When this function returns, the capturer internal state will be fully initialized
     * and the recording will be in progress.
     * The video stream will be encoded as H.264, with pixel format YUV420, while the audio stream will
     * be encoded as AAC.
     * WARNING: if there is already a recording in progress, calling this function will throw an exception.
     * If the specified output file already exists, this function will simply overwrite it without performing any check
     * @param video_device      the name of the video device to use (must be non-empty)
     * @param audio_device      the name of the audio device to use (if empty, audio won't be recorded)
     * @param output_file       the name of the output file to use to save the recording (must be non-empty)
     * @param video_params      the video dimensions (NOTE: if the width/height is set to 0, the whole display dimension
     * will be considered)
     * @return a future that can be used to check for exceptions occurring in the recording thread
     */
    std::future<void> start(const std::string &video_device, const std::string &audio_device,
                            const std::string &output_file, VideoParameters video_params);

    /**
     * Stop the recording (if the recording is already stopped, nothing will be done).
     */
    void stop();

    /**
     * Pause the recording (if the recording is already paused/stopped, nothing will be done)
     */
    void pause();

    /**
     * Resume the recording (if the recording is already proceeding/stopped, nothing will be done)
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