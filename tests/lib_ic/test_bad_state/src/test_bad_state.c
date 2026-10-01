// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "ic.h"
#include "aec.h" // for aec_h_hat_tap_index()

static ic_state_t DWORD_ALIGNED ic_state;

void test_init(int32_t conf, int32_t h_hat_exp, int32_t * h_data)
{
    ic_init(&ic_state);
    ic_state.ic_adaption_controller_state.adaption_controller_config.adaption_config = conf;
    ic_state.ic_adaption_controller_state.adaption_controller_config.enable_adaption = 1;
    ic_state.config_params.bypass = 0;

    // Set leakage_alpha to 1.0 for FORCE_OFF mode to prevent decay
    if(conf == IC_ADAPTION_FORCE_OFF) {
        ic_state.leakage_alpha.mant = (1 << 30);  // 1.0 in Q30 format
        ic_state.leakage_alpha.exp = -30;
    }

    int indx = 0;
    for(int ph = 0; ph < IC_X_CHANNELS*IC_FILTER_PHASES; ph++){
        // h_data holds the taps in time order, but h_hat stores them permuted into the bit-reversed index order
        // the FFT works in, so each tap has to be placed through aec_h_hat_tap_index()
        for(int i = 0; i < IC_FRAME_ADVANCE; i++){
            ic_state.h_hat[0][ph][aec_h_hat_tap_index(i)] = (int16_t)h_data[indx + i];
        }
        // Reinitialize BFP with the new data - this will recalculate everything properly
        bfp_s16_init(&ic_state.h_hat_bfp[0][ph], &ic_state.h_hat[0][ph][0], h_hat_exp, IC_FRAME_ADVANCE, 1);
        indx += IC_FRAME_ADVANCE;
    }
}

void test(int32_t * output, int32_t * y_frame, int32_t * x_frame)
{
    float_s32_t input_vnr_pred;
    ic_process_frame(&ic_state, y_frame, x_frame, output, &input_vnr_pred);
}
