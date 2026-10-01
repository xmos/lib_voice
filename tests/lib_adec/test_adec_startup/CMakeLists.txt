cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_adec_startup)

if(NOT DEFINED APP_HW_TARGET)
    set(APP_HW_TARGET XK-EVK-XU316)
endif()
set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../)
set(CONFIG_XSCOPE_PATH ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils)
include(${XMOS_SANDBOX_DIR}/lib_voice/tests/etc/build_options.cmake)

file(GLOB CONFIG_XSCOPE_REL_PATH
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${CONFIG_XSCOPE_PATH}/config.xscope)

file(GLOB APP_C_SRCS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/file_utils/src/*.c
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/lib_adec/test_bin_adec/src/*.c)

file(GLOB APP_XC_SRCS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/lib_adec/test_bin_adec/src/*.xc)


set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/../test_bin_adec/src
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/file_utils/src)


set(AEC_SCHEDULE_CONFIG ${TEST_ADEC_STARTUP_BUILD_CONFIG})
include(${CMAKE_CURRENT_LIST_DIR}/../../test_deps.cmake)

if (NOT BUILD_NATIVE)
    set(APP_COMPILER_FLAGS
            -report
            -Os
            -DTEST_WAV_XSCOPE=1
            ${ADEC_TEST_DE_MODE_FLAGS}
            -DTRIGGER_DE_ONLY_ON_STARTUP=1
            -DENABLE_ADEC_DEBUG_PRINTS=1)

    set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})
else()
    set(APP_COMPILER_FLAGS
            -DX86_BUILD=1
            ${ADEC_TEST_DE_MODE_FLAGS})
endif()
XMOS_REGISTER_APP()
