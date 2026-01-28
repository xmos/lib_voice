cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(test_agc_process_frame)

set(APP_HW_TARGET XK-EVK-XU316)
set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../)
include(${XMOS_SANDBOX_DIR}/lib_voice/tests/etc/build_options.cmake)

set(APP_DEPENDENT_MODULES lib_unity lib_voice)

set(APP_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/../../shared/pseudo_rand ${CMAKE_CURRENT_LIST_DIR}/../../shared/testing)

file(GLOB SHARED_SRC
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    CONFIGURE_DEPENDS
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/pseudo_rand/*.c
    ${XMOS_SANDBOX_DIR}/lib_voice/tests/shared/testing/*.c
)

file(GLOB_RECURSE TEST_C_SRCS CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_LIST_DIR} src/*.c)
set(APP_C_SRCS ${TEST_C_SRCS} ${SHARED_SRC})

file(GLOB_RECURSE tests RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS src/test*.c)

foreach(test_file ${tests})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(SOURCE_FILES_${test_name} ${test_file})
    if(NOT BUILD_NATIVE)
        set(APP_COMPILER_FLAGS_${test_name}
            -Os
            -g
            -report
            -Wall
            -fxscope
            -DDEBUG_PRINT_ENABLE=1
            -DUNITY_SUPPORT_64
            -DSPEEDUP_FACTOR=${TEST_SPEEDUP_FACTOR}
            )
    else()
        set(APP_COMPILER_FLAGS_${test_name}
            -Os
            -g
            -Wall
            -DDEBUG_PRINT_ENABLE=1
            -DUNITY_SUPPORT_64
            -DSPEEDUP_FACTOR=${TEST_SPEEDUP_FACTOR}
            )
    endif()
endforeach()

set(LIB_UNITY_AUTO_TEST_RUNNER ON)
set(LIB_UNITY_USE_FIXTURE OFF)
set(LIB_UNITY_USE_MEMORY OFF)
XMOS_REGISTER_APP()
