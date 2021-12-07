#include <iostream>

#include "../include/screen_recorder.h"

int main(int argc, char **argv) {
    std::string output_file("output.mp4");
    int framerate = 30;
    bool capture_audio = true;
    std::mutex m;
    std::condition_variable cv;
    bool pause = false;
    bool resume = false;
    bool stop = false;

    if (argc == 3) {
        output_file = argv[1];
        framerate = atoi(argv[2]);
    } else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << "[output_file] [framerate]" << std::endl;
        return 1;
    }

    ScreenRecorder sc;

    std::thread worker([&]() {
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
        }
        cv.notify_one();
    }

    if (worker.joinable()) worker.join();

    return 0;
}