// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "voice.h"
#include "profile.h"

#if AEC
extern aec_task_distribution_t tdist;
void test_aec(int32_t (*input)[AEC_FRAME_ADVANCE], int32_t (*output)[AEC_FRAME_ADVANCE])
{
    static int framenum = 0;

    // Initialise AEC
    static aec_state_t DWORD_ALIGNED aec_state;
    if(!framenum) {
        aec_init(&aec_state,
                AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS,
                AEC_MAIN_FILTER_PHASES, AEC_SHADOW_FILTER_PHASES, &tdist);
    }

    prof(0, "start_aec_process_frame");
    aec_process_frame(&aec_state, output, NULL, &input[0], &input[AEC_MAX_Y_CHANNELS]);
    prof(1, "end_aec_process_frame");
    print_prof(0, 2, framenum);
    framenum += 1;
}
#endif
