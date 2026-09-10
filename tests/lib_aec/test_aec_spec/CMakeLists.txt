cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_aec_spec)

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
    ${CMAKE_CURRENT_LIST_DIR}/../../lib_adec/test_bin_adec/src
    ${CMAKE_CURRENT_LIST_DIR}/../../shared/file_utils/src)


set(AEC_SCHEDULE_CONFIG ${TEST_AEC_SPEC_BUILD_CONFIG})

include(${CMAKE_CURRENT_LIST_DIR}/../../test_deps.cmake)

# This test shares test_bin_adec's sources, so it builds a delay estimation mode aec_conf_t even
# though it never measures a delay. The AEC here is built for the spec test's 1 y channel, 1 x
# channel, 20 main phase configuration (see parameters.cfg), which the library default 30 phase
# delay estimation filter does not fit, so declare a delay estimation filter this pool can host. The
# point is that the configuration the shared code constructs is one aec_init() could safely be
# given. This applies to both the xcore and the native build - the native build does not define
# BYPASS_ADEC, so it does not even have that to stop a delay estimation cycle - so it is set here
# rather than in either branch below.
set(TEST_AEC_SPEC_AEC_CONF_FLAGS -DADEC_DE_MODE_MAIN_FILTER_PHASES=20)

if (NOT BUILD_NATIVE)
    set(APP_COMPILER_FLAGS
            -report
            -Os
            -DTEST_WAV_XSCOPE=1
            -DBYPASS_ADEC=1
            ${TEST_AEC_SPEC_AEC_CONF_FLAGS})

    set(APP_XSCOPE_SRCS ${CONFIG_XSCOPE_REL_PATH})
else()
    set(APP_COMPILER_FLAGS
            -DX86_BUILD=1
            ${TEST_AEC_SPEC_AEC_CONF_FLAGS})
endif()
XMOS_REGISTER_APP()
