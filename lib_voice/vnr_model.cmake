## Export model
set(MODEL_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/src.autogen/vnr_model/)
set(MODEL_IN_PATH ${CMAKE_CURRENT_LIST_DIR}/src/vnr/model/trained_model.tflite)
set(MODEL_OUT_PATH ${MODEL_OUT_DIR}/trained_model_xcore.tflite)
set(MODEL_N_CORES 1)
set(MODEL_TH 0.46)

if (APP_BUILD_ARCH STREQUAL "vx4b")    
    set(ARCH_STR "VX4A")
else()
    set(ARCH_STR "XS3A")
endif()

file(MAKE_DIRECTORY ${MODEL_OUT_DIR})

add_custom_command(
    OUTPUT ${MODEL_OUT_PATH}.cpp ${MODEL_OUT_PATH}.h ${MODEL_OUT_PATH}
    COMMAND xcore-opt ${MODEL_IN_PATH} -tc ${MODEL_N_CORES} -o ${MODEL_OUT_PATH} --xcore-conv-err-threshold ${MODEL_TH} --xcore-naming-prefix vnr_model_ --xcore-target-arch=${ARCH_STR}
    DEPENDS ${MODEL_IN_PATH}
)
