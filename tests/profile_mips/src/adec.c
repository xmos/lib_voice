// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "voice.h"
#include "profile.h"

void test_adec(int32_t (*input)[AEC_FRAME_ADVANCE], int32_t (*output)[AEC_FRAME_ADVANCE])
{
    static int framenum = 0;

    static adec_config_t adec_config;
    static adec_state_t DWORD_ALIGNED adec_state;
    static adec_output_t adec_output;
    static adec_input_t adec_input;
    static aec_state_t DWORD_ALIGNED aec_state;

    if(!framenum) {
        aec_init(&aec_state,
                AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS,
                AEC_MAIN_FILTER_PHASES, AEC_SHADOW_FILTER_PHASES, &aec_tdist_chans2_threads2);
        adec_config.bypass = 0;
        adec_config.force_de_cycle_trigger = 0;
        adec_init(&adec_state, &adec_config);
    }
    aec_process_frame(&aec_state, output, NULL, &input[0], &input[AEC_MAX_Y_CHANNELS]);

    prof(0, "start_adec_estimate_delay");
    adec_estimate_delay(
        &adec_input.from_de,
        aec_state.main_state.h_hat[0],
        aec_state.main_state.num_phases
        );
    prof(1, "end_adec_estimate_delay");

    prof(2, "start_adec_process_frame");
    adec_process_frame(
            &adec_state,
            &adec_output,
            &adec_input
            );
    prof(3, "end_adec_process_frame");

    print_prof(0, 4, framenum);
    framenum += 1;

}
