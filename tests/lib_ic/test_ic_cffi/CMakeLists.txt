cmake_minimum_required(VERSION 3.21)
project(ic_py_c_frame_compare)
enable_language(CXX C ASM)

## Add lib_voice and dependencies (expected to have been fetched in the sandbox as part of xcommon cmake build)
set(SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../..)
add_subdirectory(${SANDBOX_DIR}/lib_xcore_math/lib_xcore_math ${CMAKE_BINARY_DIR}/lib_xcore_math)
add_subdirectory(${SANDBOX_DIR}/ai_tools/ai_tools ${CMAKE_BINARY_DIR}/ai_tools)
add_subdirectory(${SANDBOX_DIR}/lib_voice/lib_voice ${CMAKE_BINARY_DIR}/lib_voice)

add_library(lib_voice_module_lib_ic_cffi STATIC)
target_link_libraries(lib_voice_module_lib_ic_cffi PRIVATE lib_voice_module_lib_ic)
set_target_properties(lib_voice_module_lib_ic_cffi PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib_voice)
