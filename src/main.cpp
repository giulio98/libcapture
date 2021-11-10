#include "../include/screen_recorder.h"

using namespace std;

/* driver function to run the application */
int main() {
    ScreenRecorder screen_record;
    std::cout << "\nSelect the area to record (click to select all the display) " << std::endl;
    screen_record.SelectArea();
    screen_record.OpenInputDevices();
    screen_record.InitOutputFile();
    screen_record.CaptureFrames();

    std::cout << "\nprogram executed successfully\n";

    return 0;
}