// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "aec.h"
#include "aec_priv.h"

static aec_state_t aec_state;
void test_init()
{
    #if BUILD_NATIVE
    aec_task_distribution_t tdist = aec_tdist_chans2_threads1;
    #else
    aec_task_distribution_t tdist = aec_tdist_chans2_threads2;
    #endif

    aec_init(&aec_state, 1, 1, 9, 0, &tdist);
    aec_state.main_state.shared_state->ref_active_flag = 1;
}

void test(int32_t *output, int32_t *input)
{
    // test_calc_coherence is interested in y_hat[240:480] and prev_y[0:240]
    aec_state.main_state.shared_state->prev_y[0].data = &input[1];
    aec_state.main_state.shared_state->prev_y[0].exp = input[0];

    bfp_s32_init(&aec_state.main_state.y_hat[0], (int32_t*)&aec_state.main_state.Y_hat[0].data[0], 0, AEC_PROC_FRAME_LENGTH, 0);
    memcpy(&aec_state.main_state.y_hat[0].data[AEC_FRAME_ADVANCE], &input[AEC_FRAME_ADVANCE + 1 + 1], AEC_FRAME_ADVANCE * sizeof(int32_t));
    aec_state.main_state.y_hat[0].exp = input[AEC_FRAME_ADVANCE + 1];

    aec_calc_coherence(&aec_state.main_state, 0);

    coherence_mu_params_t *coh_mu_state_ptr = &aec_state.main_state.shared_state->coh_mu_state[0];

    memcpy(output, &coh_mu_state_ptr->coh, sizeof(float_s32_t));
    memcpy((int8_t *)output + sizeof(float_s32_t), &coh_mu_state_ptr->coh_slow, sizeof(float_s32_t));
}
