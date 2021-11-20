#include <iostream>

#include "../include/screen_recorder.h"

using namespace std;

/* driver function to run the application */
int main(int argc, char **argv) {
    int framerate = 30;

    if (argc == 2) {
        framerate = atoi(argv[1]);
        if (framerate <= 0) {
            std::cerr << "Framerate must be strictly positive" << std::endl;
            exit(1);
        }
    } else if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [framerate]" << std::endl;
        exit(1);
    }

    ScreenRecorder screen_record;
    std::string file1 = "../media/output1.mp4";
    std::string file2 = "../media/output2.mp4";

    std::cout << "Recording to " << file1 << std::endl;
    screen_record.start(file1, framerate, true);
    std::this_thread::sleep_for(15000ms);
    screen_record.pause();
    std::this_thread::sleep_for(5000ms);
    screen_record.resume();
    std::this_thread::sleep_for(1500ms);
    screen_record.stop();
    std::cout << "Recording to " << file1 << " completed" << std::endl;

    // std::this_thread::sleep_for(1000ms);
    // std::cout << std::endl;

    // std::cout << "Recording to " << file2 << std::endl;
    // screen_record.start(file2, 30, false);
    // std::this_thread::sleep_for(5000ms);
    // screen_record.pause();
    // std::this_thread::sleep_for(1000ms);
    // screen_record.stop();
    // std::cout << "Recording to " << file2 << " completed" << std::endl;

    std::cout << std::endl << "Program executed successfully" << std::endl;

    return 0;
}