// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <string.h>
#include <assert.h>

#include "aec.h"
#include "aec_priv.h"

#define Y_CH (1)
#define X_CH (1)
#define MAIN_PH (9)
#define SHADOW_PH (9)

static aec_state_t aec_state;

void test_init()
{
#if BUILD_NATIVE
    aec_task_distribution_t tdist = aec_tdist_chans2_threads1;
#else
    aec_task_distribution_t tdist = aec_tdist_chans2_threads2;
#endif

    aec_init(&aec_state, Y_CH, X_CH, MAIN_PH, SHADOW_PH, &tdist);

    // aec_state.main_state.error is normally initialised during the Error->error IFFT call.
    // Initialise here for standalone testing, reusing Error complex buffer memory.
    bfp_s32_init(&aec_state.main_state.error[0], (int32_t*)&aec_state.main_state.Error[0].data[0], 0, AEC_PROC_FRAME_LENGTH, 0);
}

void test(int32_t *output, int32_t *input)
{
    // Input layout per frame:
    // [error_exp, error_data[0:PROC_FRAME_LEN-1]]
    const int32_t error_exp = input[0];
    const int32_t *error_data = &input[1];

    bfp_s32_t *err = &aec_state.main_state.error[0];
    err->exp = error_exp;
    memcpy(err->data, error_data, AEC_PROC_FRAME_LENGTH * sizeof(int32_t));
    err->hr = bfp_s32_headroom(err);

    int32_t out_frame[1][AEC_FRAME_ADVANCE];
    aec_calc_output(&aec_state.main_state, out_frame, 0);

    // Output layout per frame:
    // [output_q31[0:AEC_FRAME_ADVANCE-1], overlap_exp, overlap_data[0:31], error_exp, error_data[0:PROC_FRAME_LEN-1]]
    unsigned o = 0;

    memcpy(&output[o], &out_frame[0][0], AEC_FRAME_ADVANCE * sizeof(int32_t));
    o += AEC_FRAME_ADVANCE;

    bfp_s32_t *ov = &aec_state.main_state.overlap[0];
    output[o++] = ov->exp;
    memcpy(&output[o], ov->data, ov->length * sizeof(int32_t));
    o += ov->length;

    output[o++] = err->exp;
    memcpy(&output[o], err->data, err->length * sizeof(int32_t));
}
