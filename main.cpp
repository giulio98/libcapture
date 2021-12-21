#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef LINUX
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#endif

#include "screen_recorder.h"

#define DEFAULT_DEVICES 0

std::tuple<int, int, int, int> parse_video_size(const std::string &str) {
    int width = 0;
    int height = 0;
    int off_x = 0;
    int off_y = 0;

    auto main_delim_pos = str.find(":");

    {
        std::string video_size = str.substr(0, main_delim_pos);
        auto delim_pos = video_size.find("x");
        if (delim_pos == std::string::npos) throw std::runtime_error("Wrong video-size format");
        width = std::stoi(video_size.substr(0, delim_pos));
        height = std::stoi(video_size.substr(delim_pos + 1));
        if (width < 0 || height < 0) throw std::runtime_error("width and height must be not-negative numbers");
    }

    if (main_delim_pos != std::string::npos) {
        std::string offsets = str.substr(main_delim_pos + 1);
        auto delim_pos = offsets.find(",");
        if (delim_pos == std::string::npos) throw std::runtime_error("Wrong offsets");
        off_x = std::stoi(offsets.substr(0, delim_pos));
        off_y = std::stoi(offsets.substr(delim_pos + 1));
        if (off_x < 0 || off_y < 0) throw std::runtime_error("offsets must be not-negative numbers");
    }

    return std::make_tuple(width, height, off_x, off_y);
}

std::tuple<std::string, std::string, int, int, int, int, int, std::string> get_params(std::vector<std::string> args) {
    int width = 0;
    int height = 0;
    int off_x = 0;
    int off_y = 0;
    int framerate = 30;
    std::string video_device;
    std::string audio_device;
    std::string output_file;

#if DEFAULT_DEVICES
#if defined(_WIN32)
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
            std::tie(width, height, off_x, off_y) = parse_video_size(*it);
            std::cout << "Parsed video size: " << width << "x" << height << std::endl;
            std::cout << "Parsed video offset: " << off_x << "," << off_y << std::endl;
            video_size_set = true;
        } else if (*it == "-f") {
            if (framerate_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            framerate = std::stoi(*it);
            framerate_set = true;
        } else if (*it == "-o") {
            if (output_set || ++it == args.end()) throw std::runtime_error(wrong_args_msg);
            output_file = *it;
            output_set = true;
        } else {
            throw std::runtime_error("Unknown arg: " + *it);
        }
    }

    std::cout << "Parsed video device: " << video_device << std::endl;
    std::cout << "Parsed audio device: " << audio_device;
#if DEFAULT_DEVICES
    if (audio_device == "none") {
        audio_device = "";
        std::cout << " (mute)";
    }
#endif
    std::cout << std::endl;

    if (!output_set) {
        output_file = "output.mp4";
        std::cout << "No output file specified, saving to " << output_file << std::endl;
    }

    return std::make_tuple(video_device, audio_device, width, height, off_x, off_y, framerate, output_file);
}

int main(int argc, char **argv) {
    int framerate;
    std::string output_file;
    std::mutex m;
    std::condition_variable cv;
    bool pause = false;
    bool resume = false;
    bool stop = false;
    int video_width, video_height, video_offset_x, video_offset_y;
    std::string video_device;
    std::string audio_device;

    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; i++) {
            args.emplace_back(argv[i]);
        }
        std::tie(video_device, audio_device, video_width, video_height, video_offset_x, video_offset_y, framerate,
                 output_file) = get_params(args);
    } catch (const std::exception &e) {
        std::string msg(e.what());
        if (msg != "") std::cerr << "ERROR: " << msg << std::endl;
        std::cerr << "Usage: " << argv[0];
        std::cerr << " [-video_device <device_name>] [-audio_device <device_name>|none]";
        std::cerr << " [-video_size <width>x<height>:<offset_x>,<offset_y>]";
        std::cerr << " [-f framerate] [-o output_file] [-h]";
        std::cerr << std::endl;
        return 1;
    }

    if (std::filesystem::exists(output_file)) {
        std::string answer;
        std::cout << "The output file " << output_file << " already exists, override it? [y/N] ";
        std::getline(std::cin, answer);
        if (answer != "y" && answer != "Y") return 0;
    }
    std::cout << std::endl;  // seprate client printing from the rest

    // try {
    //     while (true) {
    //         std::string answer;
    //         std::cout << "Do you want to record just a portion of the display? [y/n] ";
    //         std::cin >> answer;
    //         if (answer == "y") {
    //             std::cout << "Video width (0 for display size): ";
    //             std::cin >> video_width;
    //             std::cout << "Video height (0 for display size): ";
    //             std::cin >> video_height;
    //             std::cout << "Video x offset: ";
    //             std::cin >> video_offset_x;
    //             std::cout << "Video y offset: ";
    //             std::cin >> video_offset_y;
    //         } else if (answer == "n") {
    //             video_width = video_height = video_offset_x = video_offset_y = 0;
    //         } else {
    //             std::cerr << "Wrong value, enter 'y' or 'n'" << std::endl;
    //             continue;
    //         }
    //         break;
    //     }
    // } catch (const std::exception &e) {
    //     std::cerr << "ERROR: " << e.what() << std::endl;
    //     return 1;
    // }

    try {
        ScreenRecorder sc;
        sc.listAvailableDevices();

        std::thread worker([&]() {
            try {
                sc.start(video_device, audio_device, output_file, video_width, video_height, video_offset_x,
                         video_offset_y, framerate);
                while (true) {
                    std::unique_lock ul(m);
                    cv.wait(ul, [&]() { return pause || resume || stop; });
                    if (stop) {
                        sc.stop();
                        break;
                    } else if (pause) {
                        sc.pause();
                        pause = false;
                    } else if (resume) {
                        sc.resume();
                        resume = false;
                    }
                }
            } catch (const std::exception &e) {
                std::cerr << e.what() << ", terminating..." << std::endl;
                exit(1);
            }
        });

        while (!stop) {
            int input = getchar();
            std::unique_lock ul(m);
            if (input == 'p') {
                pause = true;
            } else if (input == 'r') {
                resume = true;
            } else if (input == 's') {
                stop = true;
            } else {
                if (input != 10)  // ignore CR
                    std::cerr << "Unknown option, possible options are: p[ause], r[esume], s[top]" << std::endl;
                continue;
            }
            cv.notify_one();
        }

        if (worker.joinable()) worker.join();

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