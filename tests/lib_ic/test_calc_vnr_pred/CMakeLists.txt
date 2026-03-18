cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_ic_calc_vnr_pred)

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
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/lib_vnr/vnr_unit_tests/src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/file_utils/src)

include(${CMAKE_CURRENT_LIST_DIR}/../../test_deps.cmake)

if (NOT BUILD_NATIVE)
    set(APP_COMPILER_FLAGS
            -report
            -Os
            -g
            -mcmodel=large
            -Wno-xcore-fptrgroup
            -DTEST_WAV_XSCOPE=1)

    set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})
else()
    set(APP_COMPILER_FLAGS
            -Os
            -DX86_BUILD=1)
endif()


XMOS_REGISTER_APP()
