
cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(fwk_voice_tests_ns)
if(NOT BUILD_NATIVE)
    add_subdirectory( ns_unit_tests )
endif()
add_subdirectory( test_ns_profile )


