cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)


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

target_link_libraries (screen-recorder
$<$<BOOL:${MINGW}>:mingw32>
FFmpeg::FFmpeg)


IF(LINUX)
target_link_libraries(screen-recorder ${X11_LIBRARIES})
ENDIF()