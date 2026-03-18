cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(lib_voice_examples)

if(DEFINED XCORE_TARGET)
    set(XCORE_TARGET ${XCORE_TARGET})
else()
    set(XCORE_TARGET XK-EVK-XU316)
endif()

add_subdirectory(app_aec)
add_subdirectory(app_vnr)

if(NOT BUILD_NATIVE)
    add_subdirectory(app_pipeline)
endif()
