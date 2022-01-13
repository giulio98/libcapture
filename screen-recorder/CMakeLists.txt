cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)

set(LIBRARY_NAME screen-recorder)

set(SOURCES
    src/recorder/screen_recorder.cpp
    src/format/demuxer.cpp
    src/format/muxer.cpp
    src/decode/decoder.cpp
    src/encode/encoder.cpp
    src/encode/video_encoder.cpp
    src/encode/audio_encoder.cpp
    src/convert/converter.cpp
    src/convert/video_converter.cpp
    src/convert/audio_converter.cpp
    src/process/pipeline.cpp
)

add_library(${LIBRARY_NAME} STATIC ${SOURCES})

target_include_directories(${LIBRARY_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/${LIBRARY_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)


target_include_directories(${LIBRARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


#### Platform specific configuration

if(WIN32)

    message(STATUS "Platform is Windows")
    add_definitions(-DWINDOWS)

    if(NOT MSVC)
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        set_target_properties(FFmpeg::FFmpeg
            PROPERTIES
            INTERFACE_LINK_DIRECTORIES "${screen-recorder_SOURCE_DIR}/ffmpeg/lib/"
            INTERFACE_INCLUDE_DIRECTORIES "${screen-recorder_SOURCE_DIR}/ffmpeg/include/"
            INTERFACE_LINK_LIBRARIES "avdevice;avfilter;avformat;avcodec;swresample;swscale;avutil"

        )
        file(COPY ${screen-recorder_SOURCE_DIR}/ffmpeg/bin/
                DESTINATION ${PROJECT_SOURCE_DIR}/bin)
    else()
        include(FetchContent)
        FetchContent_Declare(
            FFmpeg
            URL https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-4.3.2-full_build-shared.7z
        )
        FetchContent_GetProperties(FFmpeg)
        if(NOT ffmpeg_POPULATED)
            FetchContent_Populate(FFmpeg)
            add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
            set_target_properties(FFmpeg::FFmpeg PROPERTIES
                INTERFACE_LINK_DIRECTORIES "${ffmpeg_SOURCE_DIR}/lib"
                INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_SOURCE_DIR}/include/"
                INTERFACE_LINK_LIBRARIES "avdevice;avfilter;avformat;avcodec;swresample;swscale;avutil"
            )
            set(FFMPEG_DLLS  avdevice-58.dll avfilter-7.dll avformat-58.dll avcodec-58.dll swresample-3.dll swscale-5.dll avutil-56.dll)
            list(TRANSFORM FFMPEG_DLLS PREPEND "${ffmpeg_SOURCE_DIR}/bin/")
        endif()        
    endif()

    target_link_libraries(${LIBRARY_NAME} PRIVATE
        $<$<BOOL:${MINGW}>:mingw32>
        FFmpeg::FFmpeg
    )

    if(MSVC)
        set(INSTALL_DIR ".")
        install(TARGETS ${LIBRARY_NAME} RUNTIME DESTINATION ${INSTALL_DIR})

        set(CPACK_PACKAGE_DIRECTORY dist)
        set(CPACK_GENERATOR WIX ZIP)
        set(CPACK_PACKAGE_NAME ScreenRecorder)
        set(CPACK_PACKAGE_EXECUTABLES ${LIBRARY_NAME} ScreenRecorder)
        set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})

        include(CPack)
    endif()

else()

    if(APPLE)
        message(STATUS "Platform is macOS")
        add_definitions(-DMACOS)
    elseif(UNIX)   # LINUX
        set(LINUX TRUE)
        message(STATUS "Platform is Linux")
        add_definitions(-DLINUX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
        find_package(X11 REQUIRED)
        include_directories(${X11_INCLUDE_DIR})
        link_directories(${X11_LIBRARIES})
        target_link_libraries(${LIBRARY_NAME} ${X11_LIBRARIES})
    endif()

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
    target_link_libraries(${LIBRARY_NAME} PkgConfig::LIBAV)

endif()