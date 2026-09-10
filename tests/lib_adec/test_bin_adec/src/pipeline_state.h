// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef AP_STAGE_A_STATE_H
#define AP_STAGE_A_STATE_H

#include "aec.h"
#include "adec_state.h"
#include "delay_buffer.h"
#include "stage1.h"

/* Number of mic and reference channels the test's input wav carries, and so the width of the frames
 * this pipeline passes around. This is a property of the test audio, not of the AEC: several of the
 * tests sharing these sources feed a 4 channel (2 mic, 2 reference) wav to an AEC built for fewer y
 * channels than that, because the AEC only processes the channels its runtime configuration names.
 * Defaults to the AEC's compile time channel counts, which is right when the wav has exactly as
 * many channels as the AEC is built for; a test whose wav carries more must define these. */
#ifndef AP_MAX_Y_CHANNELS
#define AP_MAX_Y_CHANNELS (AEC_MAX_Y_CHANNELS)
#endif
#ifndef AP_MAX_X_CHANNELS
#define AP_MAX_X_CHANNELS (AEC_MAX_X_CHANNELS)
#endif

_Static_assert(AP_MAX_Y_CHANNELS >= AEC_MAX_Y_CHANNELS,
        "The wav cannot carry fewer mic channels than the AEC is built to process");
_Static_assert(AP_MAX_X_CHANNELS >= AEC_MAX_X_CHANNELS,
        "The wav cannot carry fewer reference channels than the AEC is built to process");
#define AP_FRAME_ADVANCE  (AEC_FRAME_ADVANCE)
#define AP_MAX_CHANNELS ((AP_MAX_Y_CHANNELS > AP_MAX_X_CHANNELS) ? (AP_MAX_Y_CHANNELS) : (AP_MAX_X_CHANNELS) )

typedef struct {
    // AEC
    aec_state_t DWORD_ALIGNED aec_state;

    // ADEC
    adec_state_t DWORD_ALIGNED adec_state;

    // Delay Buffer
    delay_buf_state_t delay_state;

    //Top level
    aec_conf_t aec_de_mode_conf;
    aec_conf_t aec_non_de_mode_conf;
    int32_t delay_estimator_enabled;
    int32_t adec_requested_delay_samples; // Delay requested from ADEC in case of a delay change event
    float_s32_t ref_active_threshold; //-60dB
    int32_t adec_output_delay_estimator_enabled_flag; // to keep persistant across frames
    int32_t de_output_measured_delay_samples; //for logging in test_wav
} pipeline_state_t;

#endif
