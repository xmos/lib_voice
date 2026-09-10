
cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_pipeline)

if(NOT DEFINED APP_HW_TARGET)
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

## The pipeline carries 2 mic channels in every configuration. That is STAGE1_MAX_Y_CHANNELS, which
## stage1.h derives on its own: it follows AEC_MAX_Y_CHANNELS except in ALT_ARCH_MODE, where it is 2
## because alt arch runs a 1 y channel AEC while still carrying 2 mic channels for the IC. So the
## number of y channels the AEC is built for (set per config by the AEC schedule below) is free to
## differ from the pipeline's mic count without this build having to say so.
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

## AEC compile time configuration per build config, in
## "<threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases>" format. This generates
## aec_conf.h (AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS, AEC_MAIN_FILTER_PHASES,
## AEC_SHADOW_FILTER_PHASES) and the task distribution, so that each config declares the AEC it
## actually runs rather than inheriting the library defaults. pipeline.c static asserts the values
## it expects. The runtime configuration must be a subset of these - see the preconditions on
## aec_init() - which for alt arch means 2 x channels and 15 main phases have to be built in.
set(AEC_SCHEDULE_CONFIG_std_arch      "2 2 2 10 5")
set(AEC_SCHEDULE_CONFIG_aec_ic_ns_agc "2 2 2 10 5")
set(AEC_SCHEDULE_CONFIG_alt_arch      "2 1 2 15 5")

XMOS_REGISTER_APP()
