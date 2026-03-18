set(XMOS_AITOOLSLIB_DEFINITIONS
    "TF_LITE_STATIC_MEMORY"
    "TF_LITE_STRIP_ERROR_STRINGS"
    "XCORE"
    "NO_INTERPRETER"
)

set(XMOS_AITOOLSLIB_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/libxtflitemicro_vx4a.a")
set(XMOS_AITOOLSLIB_INCLUDES "${XMOS_AITOOLSLIB_PATH}/include")


# list(APPEND APP_COMPILER_FLAGS
    # -Wfptrgroup
    # -ffunction-sections
    # -fdata-sections
    # -Wl,--gc-sections
    # -D__VX4A__=1
# )