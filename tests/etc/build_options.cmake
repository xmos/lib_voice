
# Selector to split tests into two parts for parallel build in Jenkinsfile
set(TEST_BUILD_PART "all" CACHE STRING "Which part of tests to build: all|partA|partB")
set_property(CACHE TEST_BUILD_PART PROPERTY STRINGS all partA partB)

## Factor by which to speed up unit tests
if(NOT DEFINED TEST_SPEEDUP_FACTOR)
set( TEST_SPEEDUP_FACTOR "1" CACHE STRING "Test speedup factor." )
endif()

# Each of these should be the AEC configuration the test actually runs, so that the memory pools are
# sized for it and the preconditions on aec_init() can be checked. 
#
# Note that a test's wav channel layout is not consistent, some 4ch inputs are passed to tests with
# a single y channel. This is overridden by the test's own CMakeLists configuration using
# AP_MAX_Y_CHANNELS (e.g. test_adec and test_bin_adec).

# The delay estimation tests exercise the full length delay estimation filter, rather than the
# library default of ADEC_DE_MODE_MAIN_FILTER_PHASES, which is capped at what the memory pool of a
# 10 phase AEC can hold. A delay estimation cycle is almost all X_fifo, and the pool reserves
# X_fifo for the normal mode configuration, so the AEC has to be built for 11 phases rather than 10
# for 30 to fit - see the *_BUILD_CONFIG settings below. The 1 y channel configurations already
# reserve enough X_fifo and are left alone.
set(ADEC_TEST_DE_MODE_FLAGS -DADEC_DE_MODE_MAIN_FILTER_PHASES=30)

if(NOT DEFINED DE_UNIT_TESTS_BUILD_CONFIG)
set(
    DE_UNIT_TESTS_BUILD_CONFIG
    # use 1 shadow phase to avoid 0 length array
    "2 1 1 30 1"
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
    "2 2 2 11 5"
    CACHE STRING
    "AEC build configuration for test_adec_startup in <threads> <ychannels> <xchannels> <num_main_phases> <num_shadow_phases> format"
    )
endif()

if(NOT DEFINED TEST_DELAY_ESTIMATOR_BUILD_CONFIG)
set(
    TEST_DELAY_ESTIMATOR_BUILD_CONFIG
    # use 1 shadow phase to avoid 0 length array
    "2 1 1 30 1"
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
