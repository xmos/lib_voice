
cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(vnr_unit_tests)

set(APP_HW_TARGET XK-EVK-XU316)
set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../)
set(CONFIG_XSCOPE_PATH ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils)

file(GLOB CONFIG_XSCOPE_REL_PATH
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${CONFIG_XSCOPE_PATH}/config.xscope)

file(GLOB APP_C_SRCS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/file_utils/src)

file(GLOB_RECURSE tests RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS src/test*.c)

foreach(test_file ${tests})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(SOURCE_FILES_${test_name} ${test_file})
    if (NOT BUILD_NATIVE)
        set(APP_DEPENDENT_MODULES "lib_voice"
                                    "xscope_fileio")
        set(APP_COMPILER_FLAGS_${test_name}
                -report
                -Os
                -g
                -mcmodel=large
                -Wno-xcore-fptrgroup
                -DTEST_WAV_XSCOPE=1)

        set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})
    else()
        set(APP_DEPENDENT_MODULES "lib_voice")

        set(APP_COMPILER_FLAGS_${test_name}
                -Os
                -DX86_BUILD=1)
    endif()
endforeach()

XMOS_REGISTER_APP()
