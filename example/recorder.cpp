#ifdef LINUX
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

#include <screen-recorder/screen_recorder.h>
#include <screen-recorder/video_parameters.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define DEFAULT_DEVICES 0

VideoParameters parseVideoSize(const std::string &str) {
    VideoParameters dims;

    auto main_delim_pos = str.find(":");

    {
        std::string video_size = str.substr(0, main_delim_pos);
        auto delim_pos = video_size.find("x");
        if (delim_pos == std::string::npos) throw std::runtime_error("Wrong video-size format");
        dims.width = std::stoi(video_size.substr(0, delim_pos));
        dims.height = std::stoi(video_size.substr(delim_pos + 1));
        if (dims.width < 0 || dims.height < 0)
            throw std::runtime_error("width and height must be not-negative numbers");
    }

    if (main_delim_pos != std::string::npos) {
        std::string offsets = str.substr(main_delim_pos + 1);
        auto delim_pos = offsets.find(",");
        if (delim_pos == std::string::npos) throw std::runtime_error("Wrong offsets");
        dims.offset_x = std::stoi(offsets.substr(0, delim_pos));
        dims.offset_y = std::stoi(offsets.substr(delim_pos + 1));
        if (dims.offset_x < 0 || dims.offset_y < 0) throw std::runtime_error("offsets must be not-negative numbers");
    }

    return dims;
}

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
    std::string wrong_args_msg("Wrong arguments");

    for (auto it = args.begin(); it != args.end(); it++) {
        if (*it == "-h") {
            throw std::runtime_error("");
        } else if (*it == "-video_device") {
            if (video_device_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            video_device = *it;
            video_device_set = true;
        } else if (*it == "-audio_device") {
            if (audio_device_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            audio_device = *it;
            audio_device_set = true;
        } else if (*it == "-video_size") {
            if (video_size_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            video_params = parseVideoSize(*it);
            video_size_set = true;
        } else if (*it == "-framerate") {
            if (framerate_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            framerate = std::stoi(*it);
            framerate_set = true;
        } else if (*it == "-o") {
            if (output_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            output_file = *it;
            output_set = true;
        } else if (*it == "-v") {
            verbose = true;
        } else {
            throw std::runtime_error("Unknown arg: " + *it);
        }
    }

    video_params.framerate = framerate;

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
            std::cout << "Parsed video size: " << video_params.width << "x" << video_params.height << std::endl;
            std::cout << "Parsed video offset: " << video_params.offset_x << "," << video_params.offset_y << std::endl;
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
        for (int i = 1; i < argc; i++) {
            args.emplace_back(argv[i]);
        }
        std::tie(video_device, audio_device, video_params, output_file, verbose) = parseArgs(args);
    } catch (const std::exception &e) {
        std::string msg(e.what());
        if (msg != "") std::cerr << "ERROR: " << msg << std::endl;
        std::cerr << "Usage: " << argv[0] << std::endl;
        std::cerr << "\t[-h]" << std::endl;
        std::cerr << "\t[-video_device <device_name>]" << std::endl;
        std::cerr << "\t[-audio_device <device_name>]" << std::endl;
        std::cerr << "\t[-video_size <width>x<height>:<offset_x>,<offset_y>]" << std::endl;
        std::cerr << "\t[-framerate <framerate>]" << std::endl;
        std::cerr << "\t[-o <output_file>]" << std::endl;
        std::cerr << "\t[-v]" << std::endl;
        return 1;
    }

    try {
        ScreenRecorder sc(verbose);

        if (video_device.empty()) {
            std::cerr << "ERROR: No video device specified" << std::endl << std::endl;
            sc.listAvailableDevices();
            return 1;
        }

        if (std::filesystem::exists(output_file)) {
            std::cout << "The output file '" << output_file << "' already exists, overwrite it? [y/N] ";
            std::string answer;
            std::getline(std::cin, answer);
            if (answer != "y" && answer != "Y") return 0;
        }

        sc.start(video_device, audio_device, output_file, video_params);

        bool paused = false;
        bool print_status = true;

        while (true) {
            if (print_status) printStatus(paused);
            print_status = false;
            printMenu(paused);
            std::string input;
            std::getline(std::cin, input);
            if (input.length() == 1) {
                char command = std::tolower(input.front());
                if (command == 'p' && !paused) {
                    sc.pause();
                    paused = true;
                    print_status = true;
                } else if (command == 'r' && paused) {
                    sc.resume();
                    paused = false;
                    print_status = true;
                } else if (command == 's') {
                    std::cout << "\nStopping..." << std::flush;
                    sc.stop();
                    std::cout << " done" << std::endl;
                    break;
                }
            }
            if (!print_status) std::cerr << "Unknown command" << std::endl;
        }

    } catch (const std::exception &e) {
        std::cerr << e.what() << ", terminating..." << std::endl;
        return 1;
    }

    return 0;
}

#ifdef LINUX
std::tuple<int, int, int, int> select_area() {
    int video_width, video_height, video_offset_x, video_offset_y;

    XEvent ev;
    Display *disp = nullptr;
    Screen *scr = nullptr;
    Window root = 0;
    Cursor cursor, cursor2;
    XGCValues gcval;
    GC gc;
    int rx = 0, ry = 0, rw = 0, rh = 0;
    int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
    int btn_pressed = 0, done = 0;
    int threshold = 10;

    std::cout << "Select the area to record (click to select all the display)" << std::endl;

    disp = XOpenDisplay(nullptr);
    if (!disp) throw std::runtime_error("Failed to open the display");

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
        video_width = scr->width;
        video_height = scr->height;
        video_offset_x = 0;
        video_offset_y = 0;
    } else {
        video_width = rw;
        video_height = rh;
        video_offset_x = rx;
        video_offset_y = ry;
    }

    XCloseDisplay(disp);

    return std::make_tuple(video_width, video_height, video_offset_x, video_offset_y);
}
#endif