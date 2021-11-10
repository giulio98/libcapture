#include "../include/screen_recorder.h"

using namespace std;

/* driver function to run the application */
int main() {
    ScreenRecorder screen_record;
    screen_record.start();
    std::this_thread::sleep_for(1000ms);
    screen_record.pause();
    std::this_thread::sleep_for(1000ms);
    screen_record.resume();
    std::this_thread::sleep_for(1000ms);
    screen_record.stop();

    std::cout << "\nprogram executed successfully\n";

    return 0;
}