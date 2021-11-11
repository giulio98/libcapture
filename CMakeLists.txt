cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

IF (WIN32)
    MESSAGE(STATUS "Platform is Windows")
    add_definitions(-DWINDOWS)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/lib)
    find_library(AVLIB avcodec avformat avutil avdevice avfilter postproc swresample swscale postproc REQUIRED PATHS lib)
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
    include/screen_recorder.h
    include/duration_logger.h
)

target_link_libraries(screen-recorder PkgConfig::LIBAV)

IF(LINUX)
target_link_libraries(screen-recorder ${X11_LIBRARIES})
ENDIF()