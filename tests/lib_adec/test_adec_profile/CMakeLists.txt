cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_adec_profile)

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

foreach( BUILD_CONFIG ${TEST_ADEC_PROFILE_BUILD_CONFIG} )
    set(AEC_SCHEDULE_CONFIG ${BUILD_CONFIG})
    string(REPLACE " " "_" AEC_SCHEDULE_CONFIG_ID ${AEC_SCHEDULE_CONFIG})
    set(AEC_SCHEDULE_AUTOGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/src_${AEC_SCHEDULE_CONFIG_ID}.autogen")

    if (NOT BUILD_NATIVE)
        set(APP_DEPENDENT_MODULES "lib_voice"
                                    "xscope_fileio")

        set(APP_COMPILER_FLAGS_${AEC_SCHEDULE_CONFIG_ID}
                -report
                -Os
                -DTEST_WAV_XSCOPE=1
                -DENABLE_PROFILING=1)

        set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})
    else()
        set(APP_DEPENDENT_MODULES "lib_voice")

        set(APP_COMPILER_FLAGS_${AEC_SCHEDULE_CONFIG_ID}
                -DX86_BUILD=1)
    endif()
    XMOS_REGISTER_APP()
    unset(APP_COMPILER_FLAGS_${AEC_SCHEDULE_CONFIG_ID})
endforeach()
