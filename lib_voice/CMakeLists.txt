
## Common compile options
add_library(lib_voice_compile_options INTERFACE)

if(NOT CMAKE_SYSTEM_NAME STREQUAL XCORE_XS3A)
    target_compile_options(lib_voice_compile_options INTERFACE -fPIC)
    target_compile_features(lib_voice_compile_options INTERFACE cxx_std_11)
endif()

## aec
add_library(lib_voice_module_aec INTERFACE)
include(${CMAKE_CURRENT_LIST_DIR}/aec_schedule.cmake)
target_sources(lib_voice_module_aec
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/aec/aec_impl.c
        ${CMAKE_CURRENT_LIST_DIR}/src/aec/aec_l2_impl.c
        ${CMAKE_CURRENT_LIST_DIR}/src/aec/aec_priv_impl.c
        ${CMAKE_CURRENT_LIST_DIR}/src/aec/aec_process_frame.c
        ${CMAKE_CURRENT_LIST_DIR}/src/aec/aec_schedule_defaults.c
)
target_include_directories(lib_voice_module_aec
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/api/aec
        ${CMAKE_CURRENT_LIST_DIR}/src/aec ## this is needed for the IC
)
target_compile_options(lib_voice_module_aec
    INTERFACE
        -Os
        -g
)
target_link_libraries(lib_voice_module_aec
    INTERFACE
        lib_xcore_math
)
## Create an alias
add_library(lib_voice::aec ALIAS lib_voice_module_aec)

## vnr
include(${CMAKE_CURRENT_LIST_DIR}/vnr_model.cmake)
## VNR Inference
add_library(lib_voice_module_lib_vnr STATIC)
file(GLOB_RECURSE VNR_C_SOURCES src/vnr/*.c)
file(GLOB_RECURSE VNR_CXX_SOURCES src/vnr/*.cpp)

target_sources(lib_voice_module_lib_vnr
    PRIVATE
        ${MODEL_OUT_PATH}.cpp
        ${VNR_CXX_SOURCES}
        ${VNR_C_SOURCES}
)
target_include_directories(lib_voice_module_lib_vnr
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/api/vnr
        ${CMAKE_CURRENT_LIST_DIR}/src/vnr
        ${MODEL_OUT_DIR})

target_link_libraries(lib_voice_module_lib_vnr
    PUBLIC
        lib_xcore_math
)
target_link_libraries(lib_voice_module_lib_vnr
    PRIVATE
	    ai_tools
)
target_link_libraries(lib_voice_module_lib_vnr
    PRIVATE
	    lib_voice_compile_options
)
target_compile_definitions(lib_voice_module_lib_vnr PRIVATE HEADROOM_CHECK=1)
target_compile_options(lib_voice_module_lib_vnr
    PRIVATE
        -Os
        -g
)
add_library(lib_voice::vnr ALIAS lib_voice_module_lib_vnr)

## ns
add_library(lib_voice_module_lib_ns STATIC)

target_sources(lib_voice_module_lib_ns
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/ns/ns_impl.c
        ${CMAKE_CURRENT_LIST_DIR}/src/ns/ns_low_level.c
)
target_include_directories(lib_voice_module_lib_ns
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/api/ns
        ${CMAKE_CURRENT_LIST_DIR}/src/ns
)
target_compile_options(lib_voice_module_lib_ns
    PRIVATE
        -Os
        -g
)
target_link_libraries(lib_voice_module_lib_ns
    PUBLIC
        lib_xcore_math
)
target_link_libraries(lib_voice_module_lib_ns
    PRIVATE
	    lib_voice_compile_options
)
## Create an alias
add_library(lib_voice::ns ALIAS lib_voice_module_lib_ns)

## agc
add_library(lib_voice_module_lib_agc STATIC)

target_sources(lib_voice_module_lib_agc
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/agc/agc_impl.c
)
target_include_directories(lib_voice_module_lib_agc
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/api/agc
)
target_compile_options(lib_voice_module_lib_agc
    PRIVATE
        -Os
        -g
)
target_link_libraries(lib_voice_module_lib_agc
    PUBLIC
        lib_xcore_math
)
target_link_libraries(lib_voice_module_lib_agc
    PRIVATE
	    lib_voice_compile_options
)
## Create an alias
add_library(lib_voice::agc ALIAS lib_voice_module_lib_agc)

## ic
file( GLOB_RECURSE    LIB_IC_SOURCES       ${CMAKE_CURRENT_LIST_DIR}/src/ic/*.c )
## Create library target
add_library(lib_voice_module_lib_ic STATIC)
target_sources(lib_voice_module_lib_ic
    PRIVATE
        ${LIB_IC_SOURCES}
)
target_include_directories(lib_voice_module_lib_ic
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/api/ic
        ${CMAKE_CURRENT_LIST_DIR}/src/ic
)
target_compile_options(lib_voice_module_lib_ic
    PRIVATE
        -Os
        -g
)
target_link_libraries(lib_voice_module_lib_ic
    PUBLIC
        lib_xcore_math
        lib_voice::aec
	    lib_voice::vnr
)
target_link_libraries(lib_voice_module_lib_ic
    PRIVATE
	    lib_voice_compile_options
)
## Create an alias
add_library(lib_voice::ic ALIAS lib_voice_module_lib_ic)

## adec
file( GLOB_RECURSE    LIB_ADEC_SOURCES       ${CMAKE_CURRENT_LIST_DIR}/src/adec/*.c )
## Create library target
add_library(lib_voice_module_lib_adec INTERFACE)
target_sources(lib_voice_module_lib_adec
    INTERFACE
        ${LIB_ADEC_SOURCES}
)
target_include_directories(lib_voice_module_lib_adec
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/api/adec
)
target_compile_options(lib_voice_module_lib_adec
    INTERFACE
        -Os
        -g
)
target_link_libraries(lib_voice_module_lib_adec
    INTERFACE
        lib_voice::aec
)
## Create an alias
add_library(lib_voice::adec ALIAS lib_voice_module_lib_adec)

## stage1
add_library(lib_voice_module_lib_stage1 INTERFACE)

target_sources(lib_voice_module_lib_stage1
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/stage1/delay_buffer.c
        ${CMAKE_CURRENT_LIST_DIR}/src/stage1/stage1.c
)

target_include_directories(lib_voice_module_lib_stage1
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/api/stage1
        ${CMAKE_CURRENT_LIST_DIR}/src/stage1
)

target_compile_options(lib_voice_module_lib_stage1
    INTERFACE
        -Os
        -g
)
target_link_libraries(lib_voice_module_lib_stage1
    INTERFACE
        lib_voice::adec
)
## Create an alias
add_library(lib_voice::stage1 ALIAS lib_voice_module_lib_stage1)
