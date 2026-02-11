// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "aec.h"
#include "aec_priv.h"

#define TEST_FRAME_LEN (AEC_FRAME_ADVANCE - 32)

static aec_state_t aec_state;
void test_init()
{
    #if BUILD_NATIVE
    aec_task_distribution_t tdist = aec_tdist_chans2_threads1;
    #else
    aec_task_distribution_t tdist = aec_tdist_chans2_threads2;
    #endif

    aec_init(&aec_state, 1, 1, 10, 0, &tdist);
}

void test(int32_t *output, int32_t *input)
{
    // aec_calc_corr_factor uses y_hat[240:480-32] and prev_y[0:240-32]
    aec_state.main_state.shared_state->prev_y[0].data = &input[1];
    aec_state.main_state.shared_state->prev_y[0].exp = input[0];

    bfp_s32_init(&aec_state.main_state.y_hat[0], (int32_t*)&aec_state.main_state.Y_hat[0].data[0], 0, AEC_PROC_FRAME_LENGTH, 0);
    memcpy(&aec_state.main_state.y_hat[0].data[AEC_FRAME_ADVANCE], &input[TEST_FRAME_LEN + 1 + 1], TEST_FRAME_LEN * sizeof(int32_t));
    aec_state.main_state.y_hat[0].exp = input[AEC_PROC_FRAME_LENGTH + 1];

    float_s32_t corr = aec_calc_corr_factor(&aec_state.main_state, 0);

    memcpy(output, &corr, sizeof(float_s32_t));
}
