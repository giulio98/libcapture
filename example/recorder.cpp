#ifdef LINUX
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif
#ifdef WINDOWS
#include "conio.h"
#endif

#include <libcapture/capturer.h>
#include <libcapture/video_parameters.h>
#ifndef WINDOWS
#include <poll.h>
#endif
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef WINDOWS
#include "console_setter.h"
#endif
#include "thread_guard.h"

#define DEFAULT_DEVICES 0

VideoParameters parseVideoSize(const std::string &str) {
    VideoParameters dims;

    auto main_delim_pos = str.find(':');

    {
        const std::string video_size = str.substr(0, main_delim_pos);
        auto delim_pos = video_size.find('x');
        if (delim_pos == std::string::npos) throw std::runtime_error("Wrong video-size format");
        const int width = std::stoi(video_size.substr(0, delim_pos));
        const int height = std::stoi(video_size.substr(delim_pos + 1));
        dims.setVideoSize(width, height);
    }

    if (main_delim_pos != std::string::npos) {
        const std::string offsets = str.substr(main_delim_pos + 1);
        auto delim_pos = offsets.find(',');
        if (delim_pos == std::string::npos) throw std::runtime_error("Wrong offsets");
        const int offset_x = std::stoi(offsets.substr(0, delim_pos));
        const int offset_y = std::stoi(offsets.substr(delim_pos + 1));
        dims.setVideoOffset(offset_x, offset_y);
    }

    return dims;
}

/**
 * Parse arguments
 * @param args a vector containing the arguments to parse
 * @return a tuple containing the video device name, audio device name, video parameters, output file and verboseness,
 * in this order
 */
std::tuple<std::string, std::string, VideoParameters, std::string, bool> parseArgs(std::vector<std::string> args) {
    VideoParameters video_params;
    int framerate = 30;
    std::string video_device;
    std::string audio_device;
    std::string output_file;

#if DEFAULT_DEVICES
#if defined(WINDOWS)
    video_device = "screen-capture-recorder";
    audio_device = "Gruppo microfoni (Realtek High Definition Audio(SST))";
#elif defined(LINUX)
    {
        std::stringstream ss;
        ss << getenv("DISPLAY") << ".0";
        video_device = ss.str();
    }
    audio_device = "hw:0,0";
#else
    video_device = "1";
    audio_device = "0";
#endif
#endif

    bool video_device_set = false;
    bool audio_device_set = false;
    bool video_size_set = false;
    bool framerate_set = false;
    bool output_set = false;
    bool verbose = false;
    const std::string wrong_args_msg("Wrong arguments");

    for (auto it = args.begin(); it != args.end(); it++) {
        if (*it == "--help" || *it == "-h") {
            throw std::runtime_error("");
        } else if (*it == "--video_device" || *it == "-i") {
            if (video_device_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            video_device = *it;
            video_device_set = true;
        } else if (*it == "--audio_device" || *it == "-a") {
            if (audio_device_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            audio_device = *it;
            audio_device_set = true;
        } else if (*it == "--video_size" || *it == "-s") {
            if (video_size_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            video_params = parseVideoSize(*it);
            video_size_set = true;
        } else if (*it == "--framerate" || *it == "-f") {
            if (framerate_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            framerate = std::stoi(*it);
            framerate_set = true;
        } else if (*it == "--output" || *it == "-o") {
            if (output_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            output_file = *it;
            output_set = true;
        } else if (*it == "--verbose" || *it == "-v") {
            verbose = true;
        } else {
            throw std::runtime_error("Unknown arg: " + *it);
        }
    }

    video_params.setFramerate(framerate);

    if (!output_set) {
        output_file = "output.mp4";
        std::cout << "No output file specified, saving to '" << output_file << "'" << std::endl;
    }

    if (verbose) {
        std::cout << "Parsed video device: " << video_device << std::endl;
        std::cout << "Parsed audio device: " << audio_device;
#if DEFAULT_DEVICES
        if (audio_device == "none") {
            audio_device = "";
            std::cout << " (mute)";
        }
#endif
        std::cout << std::endl;
        if (framerate_set) {
            std::cout << "Parsed framerate: " << framerate << std::endl;
        }
        if (video_size_set) {
            auto [width, height] = video_params.getVideoSize();
            auto [offset_x, offset_y] = video_params.getVideoOffset();
            std::cout << "Parsed video size: " << width << "x" << height << std::endl;
            std::cout << "Parsed video offset: " << offset_x << "," << offset_y << std::endl;
        }
        if (output_set) {
            std::cout << "Parsed output file: " << output_file << std::endl;
        }
    }

    return std::make_tuple(video_device, audio_device, video_params, output_file, verbose);
}

static void printStatus(bool paused) {
    std::cout << std::endl;
    if (paused) {
        std::cout << "Paused";
    } else {
        std::cout << "Recording...";
    }
    std::cout << std::endl;
}

static void printMenu(bool paused) {
    if (paused) {
        std::cout << "[r]esume";
    } else {
        std::cout << "[p]ause";
    }
    std::cout << ", [s]top: " << std::flush;
}

int main(int argc, char **argv) {
    VideoParameters video_params;
    std::string output_file;
    std::string video_device;
    std::string audio_device;
    bool verbose = false;

    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; i++) args.emplace_back(argv[i]);
        std::tie(video_device, audio_device, video_params, output_file, verbose) = parseArgs(args);
    } catch (const std::exception &e) {
        const std::string msg(e.what());
        if (msg != "") std::cerr << "ERROR: " << msg << std::endl;
        std::cerr << "Usage: " << argv[0];
        std::cerr << " --video_device | -i <device_name>" << std::endl;
        std::cerr << "\t[--audio_device | -a <device_name>]" << std::endl;
        std::cerr << "\t[--video_size | -s <width>x<height>:<offset_x>,<offset_y>]" << std::endl;
        std::cerr << "\t[--framerate | -f <framerate>]" << std::endl;
        std::cerr << "\t[--output | -o <output_file>]" << std::endl;
        std::cerr << "\t[--verbose | -v]" << std::endl;
        std::cerr << "\t[--help | -h]" << std::endl;
        return 1;
    }

    try {
        Capturer capturer(verbose);

        if (video_device.empty()) {
            std::cerr << "ERROR: No video device specified" << std::endl << std::endl;
            capturer.listAvailableDevices();
            return 1;
        }

        if (std::filesystem::exists(output_file)) {
            std::cout << "The output file '" << output_file << "' already exists, overwrite it? [y/N] ";
            std::string answer;
            std::getline(std::cin, answer);
            if (answer != "y" && answer != "Y") return 0;
        }

        std::atomic<bool> stopped = false;
        std::exception_ptr e_ptr;
        std::thread future_waiter;

        {  // ThreadGuard scope
            ThreadGuard tg(future_waiter);
            const std::chrono::milliseconds poll_interval(50);

            auto f = capturer.start(video_device, audio_device, output_file, video_params);
            future_waiter = std::thread([f = std::move(f), &stopped, &e_ptr, poll_interval]() mutable {
                try {
                    while (!stopped) {
                        if (f.wait_for(poll_interval) == std::future_status::ready) {
                            f.get();
                            assert(stopped);  // if we arrived here, no exception was thrown by get()
                            break;
                        };
                    }
                } catch (...) {
                    e_ptr = std::current_exception();
                    stopped = true;
                }
            });

            /* Poll STDIN to read commands */
            try {
#ifndef WINDOWS
                ConsoleSetter cs;
                struct pollfd stdin_poll = {.fd = STDIN_FILENO, .events = POLLIN};
#endif
                bool paused = false;
                bool print_status = true;
                bool print_menu = true;

                while (!stopped) {
                    if (print_status) printStatus(paused);
                    if (print_menu) printMenu(paused);
                    print_status = false;
                    print_menu = false;
#ifndef WINDOWS
                    const int poll_ret = poll(&stdin_poll, 1, 0);
#else
                    int poll_ret = _kbhit();
#endif
                    if (poll_ret > 0) {  // there's something to read
                        print_status = true;
                        print_menu = true;
#ifndef WINDOWS
                        int command = std::tolower(getchar());
#else
                        int command = std::tolower(getch());
#endif
                        if (command == 'p' && !paused) {
                            capturer.pause();
                            paused = true;
                        } else if (command == 'r' && paused) {
                            capturer.resume();
                            paused = false;
                        } else if (command == 's') {
                            std::cout << "\n\nStopping..." << std::flush;
                            stopped = true;
                            capturer.stop();
                            std::cout << " done";
                        } else {
                            if (command == '\n') command = ' ';
                            std::cerr << " Invalid command '" << (char)command << "'";
                            print_status = false;
                        }
                        std::cout << std::endl;
                    } else if (poll_ret == 0) {  // nothing to read, sleep...
                        std::this_thread::sleep_for(poll_interval);
                    } else {  // error
                        throw std::runtime_error("Failed to poll stdin");
                    }
                }
            } catch (...) {
                stopped = true;  // tell future_waiter to stop
                throw;
            }

        }  // end ThreadGuard scope: join future_waiter in any case

        if (e_ptr) std::rethrow_exception(e_ptr);

    } catch (const std::exception &e) {
        std::cerr << "\nERROR: " << e.what() << ", terminating..." << std::endl;
        return 1;
    }

    return 0;
}