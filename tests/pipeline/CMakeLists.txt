
cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_pipeline)

if(DEFINED XCORE_TARGET)
    set(APP_HW_TARGET ${XCORE_TARGET})
else()
    set(APP_HW_TARGET XK-EVK-XU316)
endif()

set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)
set(CONFIG_XSCOPE_PATH ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils)
file(GLOB CONFIG_XSCOPE_REL_PATH
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${CONFIG_XSCOPE_PATH}/config.xscope)

set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})

file(GLOB APP_C_SRCS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../shared/file_utils/src)

include(${CMAKE_CURRENT_LIST_DIR}/../test_deps.cmake)

set(APP_COMMON_FLAGS -report
                        -Os
                        -DTEST_WAV_XSCOPE=1
                        -DNUM_AEC_THREADS=2)

set(APP_COMPILER_FLAGS_std_arch
            ${APP_COMMON_FLAGS})


## Build AEC+IC+NS_AGC pipeline for testing against python.
## Everything other than initial delay estimation is run,
## which is currently not supported on the python pipeline
set(APP_COMPILER_FLAGS_aec_ic_ns_agc
            ${APP_COMMON_FLAGS}
            -DDISABLE_INITIAL_DELAY_EST=1)

set(APP_COMPILER_FLAGS_alt_arch
            ${APP_COMMON_FLAGS}
            -DALT_ARCH_MODE=1)

XMOS_REGISTER_APP()
