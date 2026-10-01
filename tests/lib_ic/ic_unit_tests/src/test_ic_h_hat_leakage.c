// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include "ic_unit_tests.h"
#include "testing.h"

#define NUM_PHASES (IC_X_CHANNELS*IC_FILTER_PHASES)

//Leaking scales every tap by the same value, so it does not care what order the taps are stored in. The reference
//below and the comparison against it can both read the stored phase directly, without aec_h_hat_tap_index().
void ic_apply_leakage_fp(
        double h_hat_fp[IC_Y_CHANNELS][NUM_PHASES][IC_FRAME_ADVANCE],
        double alpha){

    for(int ph=0; ph<NUM_PHASES; ph++){
        for(int i=0; i<IC_FRAME_ADVANCE; i++){
            h_hat_fp[0][ph][i] *= alpha;
        }
    }
}


void test_apply_leakage() {
    ic_state_t state;
    ic_init(&state);

    double h_hat_fp[IC_Y_CHANNELS][NUM_PHASES][IC_FRAME_ADVANCE] = {{{0}}};
    double alpha_fp = 0;

    unsigned seed = 45;

    for(int iter=0; iter<(1<<8)/F; iter++) {
        for(int ch=0; ch<IC_Y_CHANNELS; ch++) {
            for(int ph=0; ph<NUM_PHASES; ph++){
                state.h_hat_bfp[ch][ph].exp = pseudo_rand_int32(&seed) % 10;
                state.h_hat_bfp[ch][ph].hr = pseudo_rand_uint32(&seed) % 3;
                for(int i=0; i<IC_FRAME_ADVANCE; i++) {
                    int16_t tap = (int16_t)(pseudo_rand_int32(&seed) >> (16 + state.h_hat_bfp[ch][ph].hr));
                    state.h_hat_bfp[ch][ph].data[i] = tap;

                    h_hat_fp[ch][ph][i] = ldexp(tap, state.h_hat_bfp[ch][ph].exp);
                }
            }
        }
        //initialise leakage
        for(int ych=0; ych<IC_Y_CHANNELS; ych++) {
            state.leakage_alpha.mant = pseudo_rand_uint32(&seed) >> 1;//Positive 0 - INT_MAX
            state.leakage_alpha.exp = -31;
            alpha_fp = ldexp(state.leakage_alpha.mant,
                        state.leakage_alpha.exp);
            // printf("leakage: %f\n", alpha_fp);
        }

        for(int ych=0; ych<IC_Y_CHANNELS; ych++) {
            ic_apply_leakage(&state, ych);
            ic_apply_leakage_fp(h_hat_fp, alpha_fp);

            for(int ph=0; ph<NUM_PHASES; ph++) {
                unsigned diff = vector_int16_maxdiff(
                        &state.h_hat_bfp[ych][ph].data[0],
                        state.h_hat_bfp[ych][ph].exp,
                        &h_hat_fp[ych][ph][0],
                        0,
                        IC_FRAME_ADVANCE);
                //Three half-unit errors can stack up here: the DUT narrows leakage_alpha's 31 bit mantissa to the 16
                //bits vect_s16_scale() takes, which is up to 2^-16 relative, or half a mantissa unit over a full
                //scale 16 bit tap; the scale itself rounds; and converting the reference to compare against rounds.
                //So 2 is the worst this can be. The observed worst case is 1.
                TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(2, diff, "h_hat leakage diff too large.");
            }
        }
    }
}
