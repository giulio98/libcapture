cmake_minimum_required(VERSION 3.17)
project(screen-recorder)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)

##### FFmpeg
IF(WIN32)
set(CMAKE_INSTALL_PREFIX "${screen-recorder_SOURCE_DIR}/bin")
add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
set_target_properties(FFmpeg::FFmpeg 
        PROPERTIES
        INTERFACE_LINK_DIRECTORIES "${screen-recorder_SOURCE_DIR}/lib/"
        INTERFACE_INCLUDE_DIRECTORIES "${screen-recorder_SOURCE_DIR}/include/"
        INTERFACE_LINK_LIBRARIES "avdevice;avfilter;avformat;avcodec;swresample;swscale;avutil;"
        )
ENDIF()


IF (WIN32)
    MESSAGE(STATUS "Platform is Windows")
    add_definitions(-DWINDOWS)
ELSEIF (APPLE)
    MESSAGE(STATUS "Platform is macOS")
    add_definitions(-DMACOS)
ELSEIF (UNIX)
    set(LINUX TRUE)
    MESSAGE(STATUS "Platform is Linux")
    add_definitions(-DLINUX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
    link_directories(${X11_LIBRARIES})
ENDIF ()

if(!WIN32)
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
endif()

add_executable(
    screen-recorder
    screen-recorder/main.cpp
    screen-recorder/screen_recorder.cpp
    screen-recorder/demuxer.cpp
    screen-recorder/muxer.cpp
    screen-recorder/decoder.cpp
    screen-recorder/encoder.cpp
    screen-recorder/video_encoder.cpp
    screen-recorder/audio_encoder.cpp
    screen-recorder/video_converter.cpp
    screen-recorder/audio_converter.cpp
)
target_link_libraries(screen-recorder PRIVATE FFmpeg::FFmpeg)

IF(LINUX)
target_link_libraries(screen-recorder ${X11_LIBRARIES})
ENDIF()