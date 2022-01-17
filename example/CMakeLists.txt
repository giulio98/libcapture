cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

add_executable(recorder recorder.cpp)

target_link_libraries(recorder LINK_PUBLIC libcapture)