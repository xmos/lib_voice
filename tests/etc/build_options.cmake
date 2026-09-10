
# Selector to split tests into two parts for parallel build in Jenkinsfile
set(TEST_BUILD_PART "all" CACHE STRING "Which part of tests to build: all|partA|partB")
set_property(CACHE TEST_BUILD_PART PROPERTY STRINGS all partA partB)

## Factor by which to speed up unit tests
if(NOT DEFINED TEST_SPEEDUP_FACTOR)
set( TEST_SPEEDUP_FACTOR "1" CACHE STRING "Test speedup factor." )
endif()

# Each of these should be the AEC configuration the test actually runs, so that the memory pools are
# sized for it and the preconditions on aec_init() can be checked. Note that a test's wav channel
# layout is a separate thing: the ADEC tests feed a 4 channel (2 mic, 2 reference) wav to an AEC
# configured for fewer y channels than that, so those tests state the frame width with
# AP_MAX_Y_CHANNELS/AP_MAX_X_CHANNELS in their own CMakeLists rather than taking it from
# AEC_MAX_Y_CHANNELS/AEC_MAX_X_CHANNELS.
#
# Three of these deliberately do not follow the runtime configuration:
# - de_unit_tests runs a 1 y channel, 1 x channel, 30 main phase AEC but is built for 2 channels,
#   because it uses aec_tdist_chans2_threads2, which only exists when AEC_LIB_MAX_CHANNELS is 2.
#   Its configuration fits the 2 channel pools; see the note in its test_estimate_delay.c.
# - test_adec_startup writes an empty args.bin, so its runtime configuration is the compile time
#   default and the two cannot differ.
# - test_delay_estimator runs 0 shadow filter phases but is built for 5, so that the shadow filter
#   phase pool is not a zero length array.
if(NOT DEFINED DE_UNIT_TESTS_BUILD_CONFIG)
set(
    DE_UNIT_TESTS_BUILD_CONFIG
    "2 2 2 10 5"
    CACHE STRING
    "AEC build configuration for de_unit_tests in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED TEST_ADEC_BUILD_CONFIG)
set(
    TEST_ADEC_BUILD_CONFIG
    "2 1 2 15 5"
    CACHE STRING
    "AEC build configuration for test_adec in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED TEST_ADEC_STARTUP_BUILD_CONFIG)
set(
    TEST_ADEC_STARTUP_BUILD_CONFIG
    "2 2 2 10 5"
    CACHE STRING
    "AEC build configuration for test_adec_startup in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED TEST_DELAY_ESTIMATOR_BUILD_CONFIG)
set(
    TEST_DELAY_ESTIMATOR_BUILD_CONFIG
    "2 1 1 30 5"
    CACHE STRING
    "AEC build configuration for test_delay_estimator in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED TEST_BIN_ADEC_BUILD_CONFIG)
set(
    TEST_BIN_ADEC_BUILD_CONFIG
    "2 1 2 15 5"
    CACHE STRING
    "AEC build configuration for test_bin_adec in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED AEC_UNIT_TESTS_BUILD_CONFIG)
set(
    AEC_UNIT_TESTS_BUILD_CONFIG
    "2 2 2 10 5"
    CACHE STRING
    "AEC build configuration for aec_unit_tests in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED TEST_AEC_ENHANCEMENTS_BUILD_CONFIG)
set(
    TEST_AEC_ENHANCEMENTS_BUILD_CONFIG
    "2 2 2 10 5"
    CACHE STRING
    "AEC build configuration for test_aec_enhancements in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED TEST_AEC_SPEC_BUILD_CONFIG)
set(
    TEST_AEC_SPEC_BUILD_CONFIG
    "2 1 1 20 10"
    CACHE STRING
    "AEC build configuration for test_aec_spec in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()
