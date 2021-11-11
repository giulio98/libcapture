#include "../include/screen_recorder.h"

using namespace std;

/* driver function to run the application */
int main() {
    ScreenRecorder screen_record;
    screen_record.Start();
    std::this_thread::sleep_for(1000ms);
    screen_record.Pause();
    std::this_thread::sleep_for(1000ms);
    screen_record.Resume();
    std::this_thread::sleep_for(1000ms);
    screen_record.Stop();
    std::this_thread::sleep_for(1000ms);
    screen_record.Start();
    std::this_thread::sleep_for(1000ms);
    screen_record.Pause();
    std::this_thread::sleep_for(1000ms);
    screen_record.Stop();
    std::this_thread::sleep_for(1000ms);
    screen_record.Start();
    std::this_thread::sleep_for(1000ms);
    screen_record.Stop();
    std::this_thread::sleep_for(1000ms);
    screen_record.Resume();
    std::this_thread::sleep_for(1000ms);

    std::cout << "\nprogram executed successfully\n";

    return 0;
}