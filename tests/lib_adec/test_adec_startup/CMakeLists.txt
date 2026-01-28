cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_adec_startup)

set(APP_HW_TARGET XK-EVK-XU316)
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
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/profile/*.c
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/lib_adec/test_wav_adec/src/*.c)

file(GLOB APP_XC_SRCS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/lib_adec/test_wav_adec/src/*.xc)


set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/../test_wav_adec/src
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/file_utils/src
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/profile)


set(AEC_SCHEDULE_CONFIG ${TEST_ADEC_STARTUP_BUILD_CONFIG})

if (NOT BUILD_NATIVE)
    set(APP_DEPENDENT_MODULES "lib_voice"
                                "xscope_fileio")
    set(APP_COMPILER_FLAGS
            -report
            -Os
            -DTEST_WAV_XSCOPE=1
            -DTRIGGER_DE_ONLY_ON_STARTUP=1
            -DENABLE_ADEC_DEBUG_PRINTS=1)

    set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})
else()
    set(APP_DEPENDENT_MODULES "lib_voice")

    set(APP_COMPILER_FLAGS
            -DX86_BUILD=1)
endif()
XMOS_REGISTER_APP()
