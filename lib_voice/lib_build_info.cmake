set(LIB_NAME lib_voice)
set(LIB_VERSION 1.1.0)
set(LIB_DEPENDENT_MODULES
    "lib_xcore_math(v3.0.0)"
    "ai_tools(v1.4.3.dev40)"
)

# Size the lib_xcore_math FFT look-up tables to what lib_voice actually needs, rather than taking
# the 2^10 tables shipped in lib_xcore_math. Every transform in this library (AEC, IC, NS, VNR) is
# a 512-point real FFT, which lib_xcore_math implements as a 256-point complex transform plus a
# mono-adjust pass; that pass indexes the decimation-in-time table as far as element (512 - 5), so
# a maximum FFT length of 2^9 is the smallest that covers it. The generated coefficients are
# bit-exact with the default tables over the range used - the DIT table is indexed from its start
# and the DIF table from its end, so the smaller tables are a prefix and a suffix of the 2^10 ones.
# This halves the table memory from 16320 to 8128 bytes.
#
# lib_voice generates the tables itself, at configure time, further down this file. Both of
# lib_xcore_math's own LUT-providing branches are therefore switched off, which selects its third
# documented mode: "the user must use the provided python script to generate their own". Its
# XMATH_GEN_FFT_LUT branch cannot be used here, because it attaches an add_custom_command() to every
# app target individually while pointing them all at one output file, so a clean parallel build of a
# directory holding several app configs (aec_unit_tests has 17) runs the generator concurrently with
# other targets compiling the file it is truncating and rewriting. That loses the race as an empty
# or partial translation unit, which links as "undefined reference to xmath_dit_fft_lut".
#
# These are set as cache entries so that lib_xcore_math's build_options.cmake (which uses
# set(... CACHE ...) without FORCE) does not overwrite them; a -D on the CMake command line still
# takes precedence.
set(XMATH_MAX_FFT_LEN_LOG2 "9" CACHE STRING "Maximum FFT length to be supported by generated look-up tables. Must be a positive integer.")
set(XMATH_GEN_FFT_LUT OFF CACHE BOOL "Auto-generate FFT look-up tables.")
set(XMATH_USE_DEFAULT_FFT_LUT OFF CACHE BOOL "Use default provided FFT look-up table. (ignored if GEN_FFT_LUT is enabled).")

set(LIB_COMPILER_FLAGS
            -g
            -Os
            -DHEADROOM_CHECK=0
)

if(APP_BUILD_ARCH STREQUAL "xs3a")
    list(APPEND LIB_COMPILER_FLAGS
        -Wno-xcore-fptrgroup
    )
elseif(APP_BUILD_ARCH STREQUAL "vx4b")
    list(APPEND LIB_COMPILER_FLAGS
        -Wno-fptrgroup
    )
elseif(BUILD_NATIVE)
    list(APPEND LIB_COMPILER_FLAGS
        -D__xtflm_conf_h_exists__
        -DNN_USE_REF
    )
endif()

set(LIB_CXX_SRCS "")
include(${CMAKE_CURRENT_LIST_DIR}/vnr_model.cmake)
file(RELATIVE_PATH MODEL_OUT_DIR_REL ${CMAKE_CURRENT_LIST_DIR} ${MODEL_OUT_DIR})

set(LIB_INCLUDES
    api
    api/adec
    api/aec
    src/aec
    api/agc
    api/ic
    src/ic
    api/ns
    src/ns
    api/stage1
    api/vnr
    src/vnr
    ${MODEL_OUT_DIR_REL}
)

file(GLOB VNR_CXX_SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/vnr/*.cpp")
file(RELATIVE_PATH VNR_MODEL_SOURCES ${CMAKE_CURRENT_LIST_DIR} ${MODEL_OUT_PATH}.cpp)

list(APPEND LIB_CXX_SRCS ${VNR_MODEL_SOURCES} ${VNR_CXX_SOURCES})

XMOS_REGISTER_MODULE()

# Generate the reduced FFT look-up tables (see XMATH_MAX_FFT_LEN_LOG2 above) and hand them to every
# app target, replacing the tables lib_xcore_math would otherwise have supplied. Only the reference
# C implementations (used by native and vx4b builds) include xmath_fft_lut.h; the xs3a assembly
# refers to the tables by symbol alone, so it needs the object but not the header.
#
# Generation happens here, at configure time, rather than through an add_custom_command(). One
# directory can hold many app configs, and a build-time rule shared between them races: the
# generator truncates its output file in place while sibling targets are compiling it. Doing it at
# configure time gives every target a table that is already complete before the build starts. The
# output is keyed by table size under CMAKE_BINARY_DIR and shared by every app in the build tree, so
# it is generated once per configure however many modules ask for it.
#
# This runs after XMOS_REGISTER_MODULE() because that is what fetches lib_xcore_math into the
# sandbox; the generator script does not exist on disk until then.
if(DEFINED XMOS_DEP_DIR_lib_xcore_math)
    set(XMATH_REPO_DIR ${XMOS_DEP_DIR_lib_xcore_math})
else()
    set(XMATH_REPO_DIR ${XMOS_SANDBOX_DIR}/lib_xcore_math)
endif()
set(FFT_LUT_SCRIPT ${XMATH_REPO_DIR}/lib_xcore_math/python/gen_fft_table.py)
set(FFT_LUT_DIR ${CMAKE_BINARY_DIR}/lib_voice_xmath_fft_lut/log2_${XMATH_MAX_FFT_LEN_LOG2})
set(FFT_LUT_SRC ${FFT_LUT_DIR}/xmath_fft_lut.c)

if((NOT EXISTS ${FFT_LUT_SRC}) OR (${FFT_LUT_SCRIPT} IS_NEWER_THAN ${FFT_LUT_SRC}))
    if(NOT EXISTS ${FFT_LUT_SCRIPT})
        message(FATAL_ERROR "FFT look-up table generator not found at ${FFT_LUT_SCRIPT}")
    endif()

    # numpy is required by the generator, and comes in with ai_tools' own dependencies
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    file(MAKE_DIRECTORY ${FFT_LUT_DIR})

    message(STATUS "Generating FFT look-up tables for max FFT length 2^${XMATH_MAX_FFT_LEN_LOG2}")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${FFT_LUT_SCRIPT}
                --out_file xmath_fft_lut
                --out_dir ${FFT_LUT_DIR}
                --max_fft_log2 ${XMATH_MAX_FFT_LEN_LOG2}
                --dit
                --dif
        RESULT_VARIABLE FFT_LUT_RESULT
        ERROR_VARIABLE FFT_LUT_ERROR
    )
    if(NOT FFT_LUT_RESULT EQUAL 0)
        message(FATAL_ERROR "FFT look-up table generation failed: ${FFT_LUT_ERROR}")
    endif()
endif()

foreach(target ${APP_BUILD_TARGETS})
    target_sources(${target} PRIVATE ${FFT_LUT_SRC})
    target_include_directories(${target} PRIVATE ${FFT_LUT_DIR})
endforeach()

include(${CMAKE_CURRENT_LIST_DIR}/aec_schedule.cmake)

# Discover schedules
aec_collect_schedules(single_config_sched multi_config_name multi_config_sched)

list(LENGTH single_config_sched singleconfig_list_len)
list(LENGTH multi_config_sched multi_config_list_len)

foreach(target ${APP_BUILD_TARGETS})
    # Apply AEC schedule if needed
    if(singleconfig_list_len EQUAL 1) # App only does set(AEC_SCHEDULE_CONFIG <schedule>). Attach this schedule to all targets
        list(GET single_config_sched 0 sched)
        message(VERBOSE "Target ${target} using AEC_SCHEDULE_CONFIG schedule ${sched}")
        generate_schedule(${target} "${sched}")
    elseif(multi_config_list_len GREATER 0)
        math(EXPR _last "${multi_config_list_len} - 1")
        foreach(i RANGE 0 ${_last})
            list(GET multi_config_name ${i} config)
            if(target MATCHES "${config}$") # target ends with <config>
                list(GET multi_config_sched ${i} sched) # Get the corresponding schedule for this config
                message(VERBOSE "aec schedule config ${config}, matches target ${target} at index ${i}. schedule = ${sched}")
                generate_schedule(${target} "${sched}") # Generate schedule and add to target sources and includes
                break()
            endif()
        endforeach()
    endif()

if(BUILD_NATIVE)
    target_compile_features(${target} PRIVATE cxx_std_11)
endif()
endforeach()
