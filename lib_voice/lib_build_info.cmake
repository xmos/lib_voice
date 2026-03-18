set(LIB_NAME lib_voice)
set(LIB_VERSION 1.0.0)
set(LIB_DEPENDENT_MODULES "lib_xcore_math(lib_voice_fixes)")

set(LIB_COMPILER_FLAGS
            -g
            -Os
            -DHEADROOM_CHECK=0)

# if(BUILD_NATIVE)
#     list(APPEND LIB_COMPILER_FLAGS
#         -D__xtflm_conf_h_exists__
#         -DNN_USE_REF
#     )
# endif()

set(LIB_CXX_SRCS "")
set(lib_ASM_SRCS "")
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

    # Link aitools with the targets
    target_link_libraries(${target} PRIVATE tflite_micro)
# if(BUILD_NATIVE)
#     target_compile_features(${target} PRIVATE cxx_std_11)
# endif()
endforeach()
