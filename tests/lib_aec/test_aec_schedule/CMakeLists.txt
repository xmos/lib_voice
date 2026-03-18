cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_aec_schedule)

if(DEFINED XCORE_TARGET)
    set(APP_HW_TARGET ${XCORE_TARGET})
else()
    set(APP_HW_TARGET XK-EVK-XU316)
endif()

set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../..)

set(CONFIG_XSCOPE_PATH ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils)

file(GLOB CONFIG_XSCOPE_REL_PATH
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${CONFIG_XSCOPE_PATH}/config.xscope)

include(${CMAKE_CURRENT_LIST_DIR}/../../test_deps.cmake)


file(GLOB APP_C_SRCS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/file_utils/src)

set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})

set(COMPILER_FLAGS -Os
                   -report
                   -Wall
                   -DTEST_WAV_XSCOPE=1)

set(APP_COMPILER_FLAGS_aec_std_arch_1thread
            ${COMPILER_FLAGS})
set(AEC_SCHEDULE_CONFIG_aec_std_arch_1thread "1 2 2 10 5")

set(APP_COMPILER_FLAGS_aec_std_arch_2threads
            ${COMPILER_FLAGS})
set(AEC_SCHEDULE_CONFIG_aec_std_arch_2threads "2 2 2 10 5")

XMOS_REGISTER_APP()
