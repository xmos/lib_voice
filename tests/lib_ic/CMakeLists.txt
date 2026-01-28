
cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(fwk_voice_tests_ic)
if(NOT BUILD_NATIVE)
    add_subdirectory( ic_unit_tests )
endif()
add_subdirectory( test_ic_profile )
add_subdirectory( test_ic_spec )
add_subdirectory( characterise_c_py )
add_subdirectory( test_bad_state )
add_subdirectory( test_calc_vnr_pred )

