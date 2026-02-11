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
#define MAIN_PH 5
#define SHADOW_PH 3

#define NUM_BINS ((AEC_PROC_FRAME_LENGTH / 2) + 1)
#define BFP_LEN (NUM_BINS * 2)
#define BFP_BIN_LEN (BFP_LEN + 1) // for exponent
#define Y_LEN (Y_CH * BFP_BIN_LEN)
#define X_LEN (X_CH * MAIN_PH * BFP_BIN_LEN)
#define H_HAT_LEN (Y_CH * X_CH * MAIN_PH * BFP_BIN_LEN)

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
    aec_filter_state_t *state_ptr = &aec_state.main_state;
    for(unsigned ch = 0; ch < Y_CH; ch++){
        unsigned offset = ch * BFP_BIN_LEN;
        bfp_complex_s32_init(&state_ptr->shared_state->Y[ch], (complex_s32_t *)&input[offset + 1], input[offset], NUM_BINS, 1);
    }

    for(unsigned ch = 0; ch < X_CH; ch++) {
        for(unsigned ph = 0; ph < MAIN_PH; ph++) {
            unsigned offset = (ch * MAIN_PH + ph) * BFP_BIN_LEN + Y_LEN;
            bfp_complex_s32_init(&state_ptr->shared_state->X_fifo[ch][ph], (complex_s32_t *)&input[offset + 1], input[offset], NUM_BINS, 1);
        }
    }
    //aec init only initialises the 2d Xfifo. Since we're using the 1d fifo for error computation, call aec_update_X_fifo_1d()
    //to update the 1d Fifo
    aec_update_X_fifo_1d(state_ptr);

    for(unsigned ch = 0; ch < Y_CH; ch++) {
        for(unsigned ph = 0; ph < (X_CH * MAIN_PH); ph++) {
            unsigned offset = (ch * (X_CH * MAIN_PH) + ph) * BFP_BIN_LEN + Y_LEN + X_LEN;
            bfp_complex_s32_init(&state_ptr->H_hat[ch][ph], (complex_s32_t *)&input[offset + 1], input[offset], NUM_BINS, 1);
        }
    }

    for(unsigned ch = 0; ch < Y_CH; ch++) {
        // Y_hat is accumulated via bfp_complex_s32_macc() inside aec_l2_calc_Error_and_Y_hat().
        // The production pipeline clears Y_hat each frame in aec_process_frame(); do the same here
        // to keep frames independent and prevent drift.
        state_ptr->Y_hat[ch].exp = AEC_ZEROVAL_EXP;
        state_ptr->Y_hat[ch].hr = AEC_ZEROVAL_HR;
        memset(&state_ptr->Y_hat[ch].data[0], 0, NUM_BINS * sizeof(complex_s32_t));

        aec_calc_Error_and_Y_hat(state_ptr, ch);

        unsigned offset = ch * BFP_BIN_LEN * 2;
        unsigned offset2 = offset + BFP_BIN_LEN;

        memcpy(&output[offset], &state_ptr->Y_hat[ch].exp, sizeof(int32_t));
        memcpy(&output[offset + 1], state_ptr->Y_hat[ch].data, BFP_LEN * sizeof(int32_t));
        memcpy(&output[offset2], &state_ptr->Error[ch].exp, sizeof(int32_t));
        memcpy(&output[offset2 + 1], state_ptr->Error[ch].data, BFP_LEN * sizeof(int32_t));
    }
}
