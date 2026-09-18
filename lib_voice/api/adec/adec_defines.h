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
 * rather than derived from the application's normal mode AEC, but must fit within the same memory
 * pool.
 *
 * Applications should build their delay estimation mode `aec_conf_t` from these defines.
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

/* The delay estimator shares the same memory pool as the AEC, so check it fits. */
_Static_assert(ADEC_DE_MODE_Y_CHANNELS <= AEC_MAX_Y_CHANNELS,
        "The AEC is not built for enough y channels to run ADEC");
_Static_assert(ADEC_DE_MODE_X_CHANNELS <= AEC_MAX_X_CHANNELS,
        "The AEC is not built for enough x channels to run ADEC");
_Static_assert(ADEC_DE_MODE_X_CHANNELS * ADEC_DE_MODE_MAIN_FILTER_PHASES <= AEC_LIB_MAX_PHASES,
        "ADEC is using more filter phases than AEC_LIB_MAX_PHASES allows. Build the AEC for more "
        "phases, or reduce ADEC_DE_MODE_MAIN_FILTER_PHASES");
_Static_assert(AEC_MAIN_POOL_BYTES(ADEC_DE_MODE_Y_CHANNELS, ADEC_DE_MODE_X_CHANNELS,
                                  ADEC_DE_MODE_MAIN_FILTER_PHASES) <= sizeof(aec_memory_pool_t),
        "ADEC does not fit aec_memory_pool_t. Build the AEC for more phases, or reduce "
        "ADEC_DE_MODE_MAIN_FILTER_PHASES");
_Static_assert(AEC_SHADOW_POOL_BYTES(ADEC_DE_MODE_Y_CHANNELS, ADEC_DE_MODE_X_CHANNELS,
                                     ADEC_DE_MODE_SHADOW_FILTER_PHASES)
                <= sizeof(aec_shadow_filt_memory_pool_t),
        "ADEC does not fit aec_shadow_filt_memory_pool_t");
_Static_assert(ADEC_DE_DELAY_SAMPS <= ADEC_DE_MODE_MAIN_FILTER_PHASES * AEC_FRAME_ADVANCE,
        "The delay estimation filter is shorter than the delay range ADEC searches, so the delay "
        "ADEC applies at the start of a cycle would push the echo past the end of the filter. "
        "Increase ADEC_DE_MODE_MAIN_FILTER_PHASES or reduce ADEC_DE_DELAY_MS");

#endif
