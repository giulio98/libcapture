cmake_minimum_required(VERSION 3.17)
project(screen-recorder)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

##### FFmpeg
IF(WIN32)
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    set_target_properties(FFmpeg::FFmpeg PROPERTIES
            INTERFACE_LINK_DIRECTORIES "${screen-recorder_SOURCE_DIR}/lib/"
            INTERFACE_INCLUDE_DIRECTORIES "${screen-recorder_SOURCE_DIR}/include/"
            INTERFACE_LINK_LIBRARIES "avdevice;avfilter;avformat;avcodec;swresample;swscale;avutil;"
            )
    set(FFMPEG_DLLS avdevice-58.dll avfilter-7.dll avformat-58.dll avcodec-58.dll swresample-3.dll swscale-5.dll avutil-56.dll)
    list(TRANSFORM FFMPEG_DLLS PREPEND "${screen-recorder_SOURCE_DIR}/bin/")
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
        include/common.h
        include/deleter.h
        include/screen_recorder.h
        include/duration_logger.h
        include/demuxer.h
        include/muxer.h
        include/decoder.h
        include/encoder.h
        include/video_encoder.h
        include/audio_encoder.h
        include/video_converter.h
        include/audio_converter.h
)
set_target_properties(screen-recorder PROPERTIES
        WIN32_EXECUTABLE $<PLATFORM_ID:Windows>
        C_STANDARD 99
        )
target_link_libraries(screen-recorder PRIVATE FFmpeg::FFmpeg)

IF(LINUX)
    target_link_libraries(screen-recorder ${X11_LIBRARIES})
ENDIF()