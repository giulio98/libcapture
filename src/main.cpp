#include <iostream>
#include <string>
#include <vector>

#include "../include/screen_recorder.h"

std::tuple<int, std::string, bool> get_params(std::vector<std::string> args) {
    int framerate = 25;
    std::string output_file = "output.mp4";
    bool capture_audio = true;

    for (auto it = args.begin(); it != args.end(); it++) {
        if (*it == "-f") {
            if (++it == args.end()) throw std::runtime_error("Wrong args");
            framerate = atoi(it->c_str());
        } else if (*it == "-o") {
            if (++it == args.end()) throw std::runtime_error("Wrong args");
            output_file = *it;
        } else if (*it == "-mute") {
            capture_audio = false;
        } else {
            throw std::runtime_error("Unknown arg: " + *it);
        }
    }

    return std::make_tuple(framerate, output_file, capture_audio);
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

    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; i++) {
            args.push_back(argv[i]);
        }
        std::tie(framerate, output_file, capture_audio) = get_params(args);
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        std::cerr << "Usage: " << argv[0] << " [-f framerate] [-o output_file] [-mute]" << std::endl;
        return 1;
    }

    try {
        ScreenRecorder sc;

        std::thread worker([&]() {
            try {
                sc.start(output_file, framerate, capture_audio);
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
                std::cerr << "ERROR: " << e.what() << std::endl;
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
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}