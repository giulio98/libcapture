# screen-recorder C++ library

<p align="center">
<img alt="c++" src="https://img.shields.io/badge/C++-17-blue.svg?style=flat&logo=c%2B%2B"/> 
 <img alt="CI build" src="https://github.com/giulio98/screen-recorder/actions/workflows/build.yml/badge.svg"/> 
 <img alt="License"  src="https://img.shields.io/github/license/giulio98/screen-recorder"/> 
</p> 


screen-recorder is a multiplatform C++ library that allows to capture your screen and optionally microphone audio, build with [FFmpeg](https://github.com/FFmpeg/FFmpeg).

## Dependences
* cmake >= 2.8
* ffmpeg >= 4.3.2
* screen-capture-recorder-to-video-windows-free >= 0.12.11 (*only for Windows*) 
## Install FFmpeg
Under **Windows**, you can install FFmpeg by clicking [here](https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-4.3.2-full_build-shared.7z).
Make sure to add "C:\FFmpeg\FFmpeg\bin" on your Path variable.
Additionally, to ensure proper operation of screen-capture you need to install [screen-capture-recorder-to-video-windows-free](https://github.com/rdp/screen-capture-recorder-to-video-windows-free/releases).

Under **Linux**, you can install FFmpeg by opening your terminal and executing the following command
```bash
sudo apt-get -yq update && sudo apt-get -yq install cmake libsdl2-dev libavcodec-dev libavfilter-dev libpostproc-dev libavformat-dev libavutil-dev  libswresample-dev libswscale-dev libavdevice-dev
```

Finally, under **MacOS**, you can install FFmpeg by opening your terminal and executing the following command
```bash
brew install cmake ffmpeg
```
## Install screen-recorder

Using the cmake `FetchContent` directives you can directly setup screen-recorder as follows

```cmake
include(FetchContent)

FetchContent_Declare(
        screen-recorder
        GIT_REPOSITORY https://github.com/giulio98/screen-recorder.git
)
FetchContent_MakeAvailable(screen-recorder)
# create your executable 
# and whatever you need for
# your project ...
target_link_libraries(<your_executable> screen-recorder)
```
### Windows
After installing ffmpeg as described in [Install FFmpeg](#Install FFmpeg) your CMakeLists.txt should look like this
```cmake
cmake_minimum_required(VERSION 3.20)
project(myproject)

set(CMAKE_CXX_STANDARD 17)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include(FetchContent)

FetchContent_Declare(
        screen-recorder
        GIT_REPOSITORY https://github.com/giulio98/screen-recorder.git
)
FetchContent_MakeAvailable(screen-recorder)
add_executable(myexe main.cpp)
target_link_libraries(myexe screen-recorder)
```
As an example "main.cpp" can be [recorder.cpp](example/recorder.cpp).

At this step you will have under bin your executable
![bin-before](assets/windows-bin-before.jpg)
In order to run your executable you must add the [ffmpeg dlls](utils/ffmpeg_dlls) in the same folder of your executable.
<p align="center">
  <img src="assets/ffmpeg-dll.gif" alt="animated" />
</p>
You have done. Now you can run your executable.

