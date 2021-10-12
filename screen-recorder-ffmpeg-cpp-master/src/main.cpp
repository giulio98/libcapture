#include <bits/stdc++.h>

#include "../include/ScreenRecorder.h"

using namespace std;

/* driver function to run the application */
int main() {
    ScreenRecorder screen_record;

    screen_record.SelectArea();
    screen_record.OpenDevices();
    screen_record.InitOutputFile();
    screen_record.CaptureVideoFrames();

    cout << "\nprogram executed successfully\n";

    return 0;
}
