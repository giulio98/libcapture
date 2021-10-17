#include <bits/stdc++.h>

#include "../include/ScreenRecorder.h"

using namespace std;

/* driver function to run the application */
int main() {
    ScreenRecorder screen_record;

    screen_record.SelectArea();
    screen_record.OpenCamera();
    screen_record.OpenMic();
    screen_record.InitOutputFile();
    screen_record.CaptureVideoFrames();
    // screen_record.CaptureAudioFrames();

    cout << "\nprogram executed successfully\n";

    return 0;
}
