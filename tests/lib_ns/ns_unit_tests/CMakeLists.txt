
cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(ns_unit_tests)

set(APP_HW_TARGET XK-EVK-XU316)
set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../)
set(APP_DEPENDENT_MODULES lib_unity lib_voice)

set(APP_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/../../shared/pseudo_rand ${CMAKE_CURRENT_LIST_DIR}/../../shared/testing)
file(GLOB_RECURSE TEST_C_SRCS CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_LIST_DIR} src/*.c)

file(GLOB SHARED_SRC
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/pseudo_rand/*.c
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/testing/*.c
)

set(APP_C_SRCS ${TEST_C_SRCS} ${SHARED_SRC})

set(APP_COMPILER_FLAGS
    -Os
    -g
    -report
    -Wall
    -fxscope
    -DDEBUG_PRINT_ENABLE=1)

set(LIB_UNITY_AUTO_TEST_RUNNER OFF)
set(LIB_UNITY_USE_FIXTURE ON)
set(LIB_UNITY_USE_MEMORY ON)
XMOS_REGISTER_APP()
