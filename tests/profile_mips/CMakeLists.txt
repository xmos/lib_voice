cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(app_mips)

if(DEFINED XCORE_TARGET)
    set(APP_HW_TARGET ${XCORE_TARGET})
else()
    set(APP_HW_TARGET XK-EVK-XU316)
endif()
set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

set(CONFIG_XSCOPE_PATH ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils)

file(GLOB CONFIG_XSCOPE_REL_PATH
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${CONFIG_XSCOPE_PATH}/config.xscope)

include(${CMAKE_CURRENT_LIST_DIR}/../test_deps.cmake)


file(GLOB_RECURSE APP_C_SRCS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/profile
    ${CMAKE_CURRENT_LIST_DIR}/../shared/file_utils/src)

set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})

set(COMPILER_FLAGS -Os
                   -report
                   -Wall
                   -DENABLE_PROFILING=1
                   -DTEST_WAV_XSCOPE=1)

set(APP_COMPILER_FLAGS_aec_std_arch_1thread
            ${COMPILER_FLAGS}
            -DAEC=1)
set(AEC_SCHEDULE_CONFIG_aec_std_arch_1thread "1 2 2 10 5")

set(APP_COMPILER_FLAGS_aec_alt_arch_1thread
            ${COMPILER_FLAGS}
            -DAEC=1)
set(AEC_SCHEDULE_CONFIG_aec_alt_arch_1thread "1 1 2 15 5")

set(APP_COMPILER_FLAGS_aec_std_arch_2threads
            ${COMPILER_FLAGS}
            -DAEC=1)
set(AEC_SCHEDULE_CONFIG_aec_std_arch_2threads "2 2 2 10 5")

set(APP_COMPILER_FLAGS_aec_alt_arch_2threads
            ${COMPILER_FLAGS}
            -DAEC=1)
set(AEC_SCHEDULE_CONFIG_aec_alt_arch_2threads "2 1 2 15 5")


set(APP_COMPILER_FLAGS_ic
            ${COMPILER_FLAGS}
            -DIC=1)

set(APP_COMPILER_FLAGS_vnr
            ${COMPILER_FLAGS}
            -DVNR=1)

set(APP_COMPILER_FLAGS_agc
            ${COMPILER_FLAGS}
            -DAGC=1)

set(APP_COMPILER_FLAGS_ns
            ${COMPILER_FLAGS}
            -DNS=1)

set(APP_COMPILER_FLAGS_adec
            ${COMPILER_FLAGS}
            -DADEC=1)

XMOS_REGISTER_APP()
