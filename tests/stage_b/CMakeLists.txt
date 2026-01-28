cmake_minimum_required(VERSION 3.21)
project(test_stage_b_frame)
enable_language(CXX C ASM)
set(XCORE_MATH_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../) # Expected to have been fetched in the sandbox as part of xcommon cmake build
add_subdirectory(${XCORE_MATH_PATH}/lib_xcore_math/lib_xcore_math ${CMAKE_BINARY_DIR}/lib_xcore_math)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lib_voice ${CMAKE_BINARY_DIR}/lib_voice)
