#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "screen_recorder.h"

int select_area();

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

std::tuple<int, int, int, int, int, std::string, bool> get_params(std::vector<std::string> args) {
    int width = 0;
    int height = 0;
    int off_x = 0;
    int off_y = 0;
    int framerate = 25;
    std::string output_file = "output.mp4";
    bool capture_audio = true;

    for (auto it = args.begin(); it != args.end(); it++) {
        if (*it == "-h") {
            throw std::runtime_error("");
        } else if (*it == "-video_size") {
            if (++it == args.end()) throw std::runtime_error("Wrong args");
            std::tie(width, height, off_x, off_y) = parse_video_size(*it);
            std::cout << "Parsed video size: " << width << "x" << height << std::endl;
            std::cout << "Parsed video offset: " << off_x << "," << off_y << std::endl;
        } else if (*it == "-f") {
            if (++it == args.end()) throw std::runtime_error("Wrong args");
            framerate = std::stoi(*it);
        } else if (*it == "-o") {
            if (++it == args.end()) throw std::runtime_error("Wrong args");
            output_file = *it;
        } else if (*it == "-mute") {
            capture_audio = false;
        } else {
            throw std::runtime_error("Unknown arg: " + *it);
        }
    }

    return std::make_tuple(width, height, off_x, off_y, framerate, output_file, capture_audio);
}

int main(int argc, char **argv) {
    int framerate;
    std::string output_file;
    bool capture_audio;
    std::mutex m;
    std::condition_variable cv;
    bool pause = false;
    bool resume = false;
    bool stop = false;
    int video_width, video_height, video_offset_x, video_offset_y;

    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; i++) {
            args.emplace_back(argv[i]);
        }
        std::tie(video_width, video_height, video_offset_x, video_offset_y, framerate, output_file, capture_audio) =
            get_params(args);
    } catch (const std::exception &e) {
        std::string msg(e.what());
        if (msg != "") std::cerr << "ERROR: " << msg << std::endl;
        std::cerr
            << "Usage: " << argv[0]
            << " [-video_size <width>x<height>:<offset_x>,<offset_y>] [-f framerate] [-o output_file] [-mute] [-h]"
            << std::endl;
        return 1;
    }

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

        std::thread worker([&]() {
            try {
                sc.start(output_file, video_width, video_height, video_offset_x, video_offset_y, framerate,
                         capture_audio);
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

int select_area() {
#ifdef LINUX
    XEvent ev;
    Display *disp = NULL;
    Screen *scr = NULL;
    Window root = 0;
    Cursor cursor, cursor2;
    XGCValues gcval;
    GC gc;
    int rx = 0, ry = 0, rw = 0, rh = 0;
    int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
    int btn_pressed = 0, done = 0;
    int threshold = 10;

    std::cout << "Select the area to record (click to select all the display)" << std::endl;

    disp = XOpenDisplay(NULL);
    if (!disp) return EXIT_FAILURE;

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
        video_width_ = scr->width;
        video_height_ = scr->height;
        video_offset_x_ = 0;
        video_offset_y_ = 0;
    } else {
        video_width_ = rw;
        video_height_ = rh;
        video_offset_x_ = rx;
        video_offset_y_ = ry;
    }

    XCloseDisplay(disp);

#endif

    return 0;
}