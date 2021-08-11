# cmake_minimum_required(VERSION 3.19)
cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

add_executable(screen-recorder
    src/main.cpp
    src/ScreenRecorder.cpp
    include/ScreenRecorder.h
)

target_link_libraries(screen-recorder PkgConfig::LIBAV)