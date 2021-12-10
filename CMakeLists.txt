cmake_minimum_required(VERSION 3.16)
project(screen-recorder)

set(CMAKE_CXX_STANDARD 20)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/ffmpeg/bin)


add_executable(screen-recorder
    main.cpp
    src/screen_recorder.cpp
    src/demuxer.cpp
    src/muxer.cpp
    src/decoder.cpp
    src/encoder.cpp
    src/video_encoder.cpp
    src/audio_encoder.cpp
    src/video_converter.cpp
    src/audio_converter.cpp
)

target_include_directories(screen-recorder PRIVATE src include)


IF(WIN32)

    MESSAGE(STATUS "Platform is Windows")
    add_definitions(-DWINDOWS)

    LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

    IF(NOT MSVC)
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        set_target_properties(FFmpeg::FFmpeg
            PROPERTIES
            INTERFACE_LINK_DIRECTORIES "${screen-recorder_SOURCE_DIR}/ffmpeg/lib/"
            INTERFACE_INCLUDE_DIRECTORIES "${screen-recorder_SOURCE_DIR}/ffmpeg/include/"
            INTERFACE_LINK_LIBRARIES "avdevice;avfilter;avformat;avcodec;swresample;swscale;avutil;"
        )
    ELSE()
        INCLUDE(FetchContent)
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

ELSE()

    IF(APPLE)
        MESSAGE(STATUS "Platform is macOS")
        add_definitions(-DMACOS)
    ELSEIF(UNIX)
        set(LINUX TRUE)
        MESSAGE(STATUS "Platform is Linux")
        add_definitions(-DLINUX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

        find_package(X11 REQUIRED)
        include_directories(${X11_INCLUDE_DIR})
        link_directories(${X11_LIBRARIES})
        target_link_libraries(screen-recorder ${X11_LIBRARIES})
    ENDIF()

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
    target_link_libraries(screen-recorder PkgConfig::LIBAV)

ENDIF()


IF(WIN32)
    target_link_libraries(screen-recorder PRIVATE
        $<$<BOOL:${MINGW}>:mingw32>
        FFmpeg::FFmpeg
    )
    IF(MSVC)
        set(INSTALL_DIR ".")
        install(TARGETS screen-recorder RUNTIME DESTINATION ${INSTALL_DIR})

        set(CPACK_PACKAGE_DIRECTORY dist)
        set(CPACK_GENERATOR WIX ZIP)
        set(CPACK_PACKAGE_NAME ScreenRecorder)
        set(CPACK_PACKAGE_EXECUTABLES screen-recorder ScreenRecorder)
        set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})

        include(CPack)
    ENDIF()
ENDIF()