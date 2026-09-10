cmake_minimum_required(VERSION 3.21)
include($ENV{XMOS_CMAKE_PATH}/xcommon.cmake)
project(app_pipeline)

if(NOT DEFINED APP_HW_TARGET)
    set(APP_HW_TARGET XK-EVK-XU316)
endif()

set(APP_COMPILER_FLAGS_std_arch -report)
set(APP_COMPILER_FLAGS_alt_arch -report -DALT_ARCH_MODE=1)

## AEC compile time configuration per build config, in
## "<threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases>" format. This generates
## aec_conf.h (AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS, AEC_MAIN_FILTER_PHASES,
## AEC_SHADOW_FILTER_PHASES) and the task distribution, so that each config declares the AEC it
## runs. The runtime configuration must be a subset of these - see the preconditions on aec_init().
## alt arch runs a 1 y channel, 2 x channel, 15 main phase AEC, and both configs additionally run a
## 1 y channel, 1 x channel, 30 main phase AEC while ADEC estimates the delay, so the pool has to
## cover num_x_channels * num_main_filter_phases = 30 phases of X_fifo either way.
set(AEC_SCHEDULE_CONFIG_std_arch "1 2 2 10 5")
set(AEC_SCHEDULE_CONFIG_alt_arch "1 1 2 15 5")

set(APP_DEPENDENT_MODULES lib_voice)

set(XMOS_SANDBOX_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

XMOS_REGISTER_APP()
