// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef ADEC_DEFINES_H
#define ADEC_DEFINES_H

#include "aec.h"

/**
 * @defgroup adec_defines   ADEC #define constants
 */

/** 
 * @brief Number of frames far we look back to smooth the peak to average filter power ratio history
 * @ingroup adec_defines
 */
#define ADEC_PEAK_TO_AVERAGE_HISTORY_DEPTH         8

/**
 * @brief Number of frames of peak power history we look at while computing AEC goodness metric. NOT USER MODIFIABLE 
 * @ingroup adec_defines
 */
#define ADEC_PEAK_LINREG_HISTORY_SIZE           66

/**
 * @brief Initial delay of microphone in the DE mode in milliseconds.
 * This allows measuring up to `ADEC_DE_DELAY_MS` ms of delay in cases when the mic is earlier than the reference.
 * 
 * @ingroup adec_defines
 */
#define ADEC_DE_DELAY_MS                        150


/**
 * @brief Same as `ADEC_DE_DELAY_MS` but in samples.
 * 
 * @ingroup adec_defines
 */
#define ADEC_DE_DELAY_SAMPS                     (16000 * ADEC_DE_DELAY_MS / 1000)


/**
 * @brief The AEC configuration ADEC runs during a delay estimation cycle.
 *
 * While estimating the delay, ADEC has the application re-initialise the AEC as a single channel
 * filter long enough to search the delay range, with no shadow filter. This configuration is fixed
 * rather than derived from the application's normal mode AEC, so it is not bounded by
 * @ref AEC_MAIN_FILTER_PHASES: `ADEC_DE_MODE_X_CHANNELS * ADEC_DE_MODE_MAIN_FILTER_PHASES` can be
 * larger than `AEC_MAX_X_CHANNELS * AEC_MAIN_FILTER_PHASES`, which is why it has to be checked
 * against the memory pool explicitly - see the assertions below.
 *
 * Applications should build their delay estimation mode `aec_conf_t` from these rather than
 * repeating the numbers, so that the configuration and the checks cannot disagree.
 *
 * @ingroup adec_defines
 */
#ifndef ADEC_DE_MODE_Y_CHANNELS
#define ADEC_DE_MODE_Y_CHANNELS                 (1)
#endif
/** @brief See @ref ADEC_DE_MODE_Y_CHANNELS @ingroup adec_defines */
#ifndef ADEC_DE_MODE_X_CHANNELS
#define ADEC_DE_MODE_X_CHANNELS                 (1)
#endif
/** @brief See @ref ADEC_DE_MODE_Y_CHANNELS @ingroup adec_defines */
#ifndef ADEC_DE_MODE_MAIN_FILTER_PHASES
#define ADEC_DE_MODE_MAIN_FILTER_PHASES         (30)
#endif
/** @brief See @ref ADEC_DE_MODE_Y_CHANNELS @ingroup adec_defines */
#ifndef ADEC_DE_MODE_SHADOW_FILTER_PHASES
#define ADEC_DE_MODE_SHADOW_FILTER_PHASES       (0)
#endif

/* A delay estimation cycle re-initialises the AEC with the configuration above, using the same
 * compile time memory pools as the application's normal mode AEC. Nothing about the normal mode
 * configuration implies that the delay estimation one fits, so check it here: any build that links
 * ADEC can reach a delay estimation cycle, and getting this wrong overruns the pools silently.
 * Adjusting ADEC_DE_MODE_MAIN_FILTER_PHASES, or building the AEC for fewer channels or phases, will
 * fail here instead. */
_Static_assert(ADEC_DE_MODE_Y_CHANNELS <= AEC_MAX_Y_CHANNELS,
        "The AEC is not built for enough y channels to run a delay estimation cycle");
_Static_assert(ADEC_DE_MODE_X_CHANNELS <= AEC_MAX_X_CHANNELS,
        "The AEC is not built for enough x channels to run a delay estimation cycle");
_Static_assert(ADEC_DE_MODE_X_CHANNELS * ADEC_DE_MODE_MAIN_FILTER_PHASES <= AEC_LIB_MAX_PHASES,
        "A delay estimation cycle indexes more filter phases than AEC_LIB_MAX_PHASES, so it would "
        "run off the end of aec_filter_state_t::h_hat, aec_filter_state_t::X_fifo_1d and "
        "de_output_t::phase_power. Build the AEC for more phases, or reduce "
        "ADEC_DE_MODE_MAIN_FILTER_PHASES");
_Static_assert(AEC_MAIN_POOL_BYTES(ADEC_DE_MODE_Y_CHANNELS, ADEC_DE_MODE_X_CHANNELS,
                                  ADEC_DE_MODE_MAIN_FILTER_PHASES) <= sizeof(aec_memory_pool_t),
        "A delay estimation cycle does not fit aec_memory_pool_t. The pool reserves "
        "AEC_MAX_X_CHANNELS * AEC_MAIN_FILTER_PHASES phases of X_fifo, which does not bound the "
        "ADEC_DE_MODE_X_CHANNELS * ADEC_DE_MODE_MAIN_FILTER_PHASES a delay estimation cycle needs. "
        "Build the AEC for more phases, or reduce ADEC_DE_MODE_MAIN_FILTER_PHASES");
_Static_assert(AEC_SHADOW_POOL_BYTES(ADEC_DE_MODE_Y_CHANNELS, ADEC_DE_MODE_X_CHANNELS,
                                     ADEC_DE_MODE_SHADOW_FILTER_PHASES)
                <= sizeof(aec_shadow_filt_memory_pool_t),
        "A delay estimation cycle does not fit aec_shadow_filt_memory_pool_t");
_Static_assert(ADEC_DE_DELAY_SAMPS <= ADEC_DE_MODE_MAIN_FILTER_PHASES * AEC_FRAME_ADVANCE,
        "The delay estimation filter is shorter than the delay range ADEC searches, so the delay "
        "ADEC applies at the start of a cycle would push the echo past the end of the filter. "
        "Increase ADEC_DE_MODE_MAIN_FILTER_PHASES or reduce ADEC_DE_DELAY_MS");

#endif
