// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "aec.h"
#include "aec_priv.h"

#define Y_CH 1
#define X_CH 2
#define MAIN_PH 6
#define SHADOW_PH 2

#define NUM_BINS ((AEC_PROC_FRAME_LENGTH / 2) + 1)
#define BFP_BIN_LEN (NUM_BINS + 1) // for exponent

static aec_state_t aec_state;
void test_init()
{
    #if BUILD_NATIVE
    aec_task_distribution_t tdist = aec_tdist_chans2_threads1;
    #else
    aec_task_distribution_t tdist = aec_tdist_chans2_threads2;
    #endif

    aec_init(&aec_state, Y_CH, X_CH, MAIN_PH, SHADOW_PH, &tdist);
}

void test(int32_t *output, int32_t *input)
{
    int32_t is_shadow = input[0];
    float_s32_t delta = {input[1], input[2]};

    aec_filter_state_t *state_ptr = (is_shadow) ? &aec_state.shadow_state : &aec_state.main_state;

    state_ptr->delta = delta;
    for(unsigned ch = 0; ch < X_CH; ch++) {
        unsigned offset = 3 + ch * BFP_BIN_LEN;
        // unsigned offset2 = 3 
        int32_t scratch[NUM_BINS] = {0};
        bfp_s32_init(&state_ptr->shared_state->sigma_XX[ch], &input[offset + 1], input[offset], NUM_BINS, 1);
        offset += BFP_BIN_LEN * 2;
        bfp_s32_init(&state_ptr->X_energy[ch], &input[offset + 1], input[offset], NUM_BINS, 1);
        bfp_s32_init(&state_ptr->inv_X_energy[ch], scratch, 0, NUM_BINS, 0);

        aec_calc_normalisation_spectrum(state_ptr, ch, is_shadow);

        unsigned out_offset = ch * BFP_BIN_LEN;
        memcpy(&output[out_offset], &state_ptr->inv_X_energy[ch].exp, sizeof(int32_t));
        memcpy(&output[out_offset + 1], state_ptr->inv_X_energy[ch].data, NUM_BINS * sizeof(int32_t));
    }
}
