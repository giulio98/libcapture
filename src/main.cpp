#include "../include/ScreenRecorder.h"

using namespace std;

/* driver function to run the application */
int main() {
    ScreenRecorder screen_record;
    cout << "\nSelect the area to record (click to select all the display) " << endl;
    // screen_record.selectArea();
    screen_record.openCamera();
    screen_record.init_outputfile();
    screen_record.CaptureVideoFrames();

    cout << "\nprogram executed successfully\n";

    return 0;
}