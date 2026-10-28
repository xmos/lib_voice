// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "voice.h"
#include "profile.h"

void test_ic(int32_t (*input)[IC_FRAME_ADVANCE], int32_t (*output)[IC_FRAME_ADVANCE])
{
    static int framenum = 0;
    static ic_state_t DWORD_ALIGNED ic_state;
    static float_s32_t input_vnr_pred;

    if(!framenum) {
        ic_init(&ic_state);
#if DISABLE_ADAPTION_CONTROLLER
        ic_state.ic_adaption_controller_state.adaption_controller_config.adaption_config = IC_ADAPTION_FORCE_ON;
        ic_state.leakage_alpha = f32_to_float_s32(1.0); //From test_bin_ic
#endif
    }

    prof(0, "start_ic_process_frame");
    ic_process_frame(&ic_state, output[0], input[0], input[1], &input_vnr_pred, 0);
    prof(1, "end_ic_process_frame");
    print_prof(0, 2, framenum);
    framenum += 1;
}
