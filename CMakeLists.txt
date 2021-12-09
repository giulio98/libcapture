cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

IF (WIN32)
    MESSAGE(STATUS "Platform is Windows")
    add_definitions(-DWINDOWS)
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    set_target_properties(FFmpeg::FFmpeg
            PROPERTIES
            INTERFACE_LINK_DIRECTORIES "${screen-recorder_SOURCE_DIR}/lib/"
            INTERFACE_INCLUDE_DIRECTORIES "${screen-recorder_SOURCE_DIR}/include/"
            INTERFACE_LINK_LIBRARIES "avdevice;avfilter;avformat;avcodec;swresample;swscale;avutil;"
            )
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

IF(NOT WIN32)
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
ENDIF()

add_executable(screen-recorder
    src/main.cpp
    src/screen_recorder.cpp
    src/demuxer.cpp
    src/muxer.cpp
    src/decoder.cpp
    src/encoder.cpp
    src/video_encoder.cpp
    src/audio_encoder.cpp
    src/video_converter.cpp
    src/audio_converter.cpp
    src/include/common.h
    src/include/deleter.h
    src/include/screen_recorder.h
    src/include/duration_logger.h
    src/include/demuxer.h
    src/include/muxer.h
    src/include/decoder.h
    src/include/encoder.h
    src/include/video_encoder.h
    src/include/audio_encoder.h
    src/include/video_converter.h
    src/include/audio_converter.h
)

IF(WIN32)
    target_link_libraries(screen-recorder FFmpeg::FFmpeg)
ENDIF()

IF(NOT WIN32)
target_link_libraries(screen-recorder PkgConfig::LIBAV)
ENDIF()

IF(LINUX)
target_link_libraries(screen-recorder ${X11_LIBRARIES})
ENDIF()