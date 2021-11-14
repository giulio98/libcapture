cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

IF (WIN32)
    MESSAGE(STATUS "Platform is Windows")
    add_definitions(-DWINDOWS)
ELSEIF (APPLE)
    MESSAGE(STATUS "Platform is macOS")
    add_definitions(-DMACOS)
ELSEIF (UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
    set(LINUX TRUE)
    MESSAGE(STATUS "Platform is Linux")
    add_definitions(-DLINUX)

    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
    link_directories(${X11_LIBRARIES})
ENDIF ()

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
    src/screen_recorder.cpp
    src/demuxer.cpp
    src/muxer.cpp
    src/decoder.cpp
    src/encoder.cpp
    src/video_encoder.cpp
    src/audio_encoder.cpp
    src/video_converter.cpp
    include/ffmpeg_libs.h
    include/screen_recorder.h
    include/duration_logger.h
    include/demuxer.h
    include/muxer.h
    include/decoder.h
    include/encoder.h
    include/video_encoder.h
    include/audio_encoder.h
    include/video_converter.h
)

target_link_libraries(screen-recorder PkgConfig::LIBAV)

IF(LINUX)
target_link_libraries(screen-recorder ${X11_LIBRARIES})
ENDIF()