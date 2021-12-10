cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

IF (WIN32)
    MESSAGE(STATUS "Platform is Windows")
    add_definitions(-DWINDOWS)
    if(NOT MSVC)
        find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
    ELSE()
        FetchContent_Declare(
                FFmpeg
                URL https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-4.3.2-full_build-shared.7z
        )
        FetchContent_GetProperties(FFmpeg)
        IF(NOT ffmpeg_POPULATED)
            FetchContent_Populate(FFmpeg)
            add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
            set_target_properties(FFmpeg::FFmpeg PROPERTIES
                    INTERFACE_LINK_DIRECTORIES "${ffmpeg_SOURCE_DIR}/lib"
                    INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_SOURCE_DIR}/include/"
                    INTERFACE_LINK_LIBRARIES "avdevice;avfilter;avformat;avcodec;swresample;swscale;avutil;"
                    )
            set(FFMPEG_DLLS  avdevice-58.dll avfilter-7.dll avformat-58.dll avcodec-58.dll swresample-3.dll swscale-5.dll avutil-56.dll)
            list(TRANSFORM FFMPEG_DLLS PREPEND "${ffmpeg_SOURCE_DIR}/bin/")
        ENDIF()
    ENDIF()
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
    target_link_libraries(screen-recorder PRIVATE
            $<$<BOOL:${MINGW}>:mingw32>
            FFmpeg::FFmpeg)
    IF(MSVC)
        add_custom_command(TARGET screen-recorder POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FFMPEG_DLLS}
                $<TARGET_FILE_DIR:screen-recorder>
                )
    ENDIF()
ENDIF()

IF(NOT WIN32)
target_link_libraries(screen-recorder PkgConfig::LIBAV)
ENDIF()

IF(LINUX)
target_link_libraries(screen-recorder ${X11_LIBRARIES})
ENDIF()