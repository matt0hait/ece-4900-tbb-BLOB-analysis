cmake_minimum_required(VERSION 3.17)

# Definitions
set(PROJECT_NAME ECE-4900_FINAL_M_Hait)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_GLIBCXX_USE_CXX17_ABI=0)
set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")
project(${PROJECT_NAME})
list(APPEND SOURCE_FILES
        main.cpp
        include/utils/img_utils.c
        include/utils/vision_utils.cpp
        include/utils/ffmpeg_utils.cpp
        include/utils/ffmpeg_utils.h
        include/utils/cxxopts.hpp
        )

include_directories(include/utils)
add_subdirectory(include/FFmpeg)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} m tbb FFmpeg )
