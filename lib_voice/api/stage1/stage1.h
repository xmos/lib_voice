// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef STAGE1_STATE_H
#define STAGE1_STATE_H

#include "aec.h"
#include "adec.h"
#include "delay_buffer.h"

/**
 * @defgroup stage1_api     Stage1 API
 */

/**
 * @defgroup stage1_types     Stage1 types
 */

/** Enable stage1 alternative arch mode
 *
 * @ingroup stage1_types
 */
#ifndef ALT_ARCH_MODE
#define ALT_ARCH_MODE (0)
#endif


/** Number of microphone (Y) channels the pipeline carries through stage1.
 *
 * This is the width of the mic input and output frames stage1 works on. It is not necessarily the
 * number of channels the AEC is configured for: in alt arch mode the AEC runs on a single mic
 * channel while the pipeline carries two, because the downstream interference canceller needs both
 * with their phase relationship preserved. Stage1 fills the channels the AEC did not process by
 * duplicating the AEC output or by copying the mic input, so it has to know how many the pipeline
 * expects.
 *
 * In @ref ALT_ARCH_MODE this is 2 and does not follow @ref AEC_MAX_Y_CHANNELS, because alt arch is
 * a two mic design by construction: the AEC processes one mic channel and stage1 fills the second
 * for the IC, so alt arch cannot work with a single mic channel however the AEC is configured.
 * Otherwise it defaults to @ref AEC_MAX_Y_CHANNELS, which is correct for any pipeline that feeds
 * every mic channel it carries through the AEC.
 *
 * @ingroup stage1_types
 */
#ifndef STAGE1_MAX_Y_CHANNELS
#if ALT_ARCH_MODE
#define STAGE1_MAX_Y_CHANNELS (2)
#else
#define STAGE1_MAX_Y_CHANNELS (AEC_MAX_Y_CHANNELS)
#endif
#endif

_Static_assert(STAGE1_MAX_Y_CHANNELS >= AEC_MAX_Y_CHANNELS,
        "STAGE1_MAX_Y_CHANNELS is the number of mic channels the pipeline carries, so it cannot be "
        "smaller than the number of y channels the AEC can be configured for");
_Static_assert(STAGE1_MAX_Y_CHANNELS <= MAX_DELAY_BUF_CHANNELS,
        "The delay buffer holds MAX_DELAY_BUF_CHANNELS channels, so it cannot delay every mic "
        "channel the pipeline carries");
#if ALT_ARCH_MODE
_Static_assert(STAGE1_MAX_Y_CHANNELS >= 2,
        "alt arch duplicates the single channel AEC output into a second mic channel for the IC, so "
        "it needs the pipeline to carry at least 2 mic channels");
#endif


/** Limit in seconds for which AEC is kept enabled after detecting reference as inactive.
 *  Used only in alt arch configuration.
 *
 * @ingroup stage1_types
 */
#define HOLD_AEC_LIMIT_SECONDS (3)


/**
 * @brief AEC runtime configuration structure
 *
 * @ingroup stage1_types
 */
typedef struct {
    /**< Number of reference (X) input channels at runtime */
    uint8_t num_x_channels;
    /**< Number of microphone (Y) input channels at runtime */
    uint8_t num_y_channels;
    /** Runtime main-filter phase count */
    uint8_t num_main_filt_phases;
    /** Runtime shadow-filter phase count */
    uint8_t num_shadow_filt_phases;
    /** Pointer to the work distribution schedule to use (e.g., @ref aec_tdist_chans2_threads1) */
    const aec_task_distribution_t * tdist;
} aec_conf_t;

/**
 * @brief Persistent state for stage1 processing
 *
 * It aggregates AEC, ADEC and delay buffer state, AEC runtime configurations for
 * delay and non-delay estimation mode and control flags used in stage1 processing.
 *
 * @ingroup stage1_types
 */
typedef struct {
    /** AEC state */
    aec_state_t DWORD_ALIGNED aec_state;

    /** ADEC state */
    adec_state_t DWORD_ALIGNED adec_state;

    /** Delay buffer state */
    delay_buf_state_t DWORD_ALIGNED delay_state;

    /** AEC config in delay estimation mode */
    aec_conf_t aec_de_mode_conf;

    /** AEC config in non delay estimation (regular) mode */
    aec_conf_t aec_non_de_mode_conf;

    /** Flag indicating if AEC is running in delay estimation mode */
    int32_t delay_estimator_enabled;

    /** Threshold used for detecting activity on the reference audio channel */
    float_s32_t ref_active_threshold;

    /** Number of consecutive frames reference has been inactive for.
     * Used only in alt-arch mode
     */
    int32_t hold_aec_count;

    /** Number of frames the reference must be inactive before AEC is disabled.
     * Used only in alt-arch mode
     */
    int32_t hold_aec_limit;
} stage1_t;

/**
 * @brief Initialise Stage1 processing.
 *
 * Sets up persistent state for Stage1, initialises ADEC, AEC (in non delay estimation mode)
 * and the delay buffer.
 * Also resets internal counters used to control AEC enable/disable behaviour in
 * alt-arch mode.
 *
 * All pointers must be non-NULL. The @ref stage1_t memory must persist for the lifetime
 * of the stage 1 processing.
 *
 * @param[in,out] state         Stage1 state to initialise.
 * @param[in]     de_conf       AEC runtime configuration used when the delay
 *                              estimator path is enabled.
 * @param[in]     non_de_conf   AEC runtime configuration running in non delay estimation
 *                              mode
 * @param[in]     adec_config   ADEC configuration
 *
 * @ingroup stage1_api
 */
void stage1_init(stage1_t *state, aec_conf_t *de_conf, aec_conf_t *non_de_conf, adec_config_t *adec_config);

/**
 * @brief Performs stage1 processing on a frame of input data
 *
 * This function delays the input by the estimated delay, performs AEC processing on the
 * input frame, updates metadata propagated to downstream pipeline stages,
 * runs ADEC and applies ADEC result (e.g. switch AEC config,
 * change applied delay).
 *
 * Supports standard or alternating-architecture mode controlled by
 * the compile-time flag @ref ALT_ARCH_MODE.
 *
 * @param[in,out] state            Persistent Stage1 state.
 * @param[out]    output_frame     Output frame buffer [@ref STAGE1_MAX_Y_CHANNELS][AEC_FRAME_ADVANCE] in Q31 format.
 * @param[out]    max_ref_energy   Maximum reference-channel energy (float_s32_t) for this frame.
 * @param[out]    aec_corr_factor  AEC correction factor (float_s32_t) computed for this frame.
 * @param[out]    ref_active_flag  Set non-zero if reference is detected active this frame.
 * @param[in]     input_y          Microphone (Y) input frame [@ref STAGE1_MAX_Y_CHANNELS][AEC_FRAME_ADVANCE] in Q31 format.
 * @param[in]     input_x          Reference (X) input frame [X channels][AEC_FRAME_ADVANCE] in Q31 format.
 *
 * @ingroup stage1_api
 */
void stage1_process_frame(stage1_t *state, int32_t (*output_frame)[AEC_FRAME_ADVANCE],
    float_s32_t *max_ref_energy, float_s32_t *aec_corr_factor, int32_t *ref_active_flag,
    int32_t (*input_y)[AEC_FRAME_ADVANCE], int32_t (*input_x)[AEC_FRAME_ADVANCE]);
#endif
