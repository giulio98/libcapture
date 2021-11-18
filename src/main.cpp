#include <iostream>

#include "../include/screen_recorder.h"

using namespace std;

/* driver function to run the application */
int main() {
    ScreenRecorder screen_record;
    std::string file1 = "../media/output1.mp4";
    std::string file2 = "../media/output2.mp4";

    std::cout << "Recording to " << file1 << std::endl;
    screen_record.start(file1, 25, true);
    std::this_thread::sleep_for(5000ms);
    screen_record.pause();
    std::this_thread::sleep_for(5000ms);
    screen_record.resume();
    std::this_thread::sleep_for(5000ms);
    screen_record.stop();
    std::cout << "Recording to " << file1 << " completed" << std::endl;

    std::this_thread::sleep_for(1000ms);
    std::cout << std::endl;

    std::cout << "Recording to " << file2 << std::endl;
    screen_record.start(file2, 30, false);
    std::this_thread::sleep_for(5000ms);
    screen_record.pause();
    std::this_thread::sleep_for(1000ms);
    screen_record.stop();
    std::cout << "Recording to " << file2 << " completed" << std::endl;

    std::cout << std::endl << "Program executed successfully" << std::endl;

    return 0;
}