// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "voice.h"

/**
 * @file
 * @brief Memory profile harness for lib_voice modules.
 *
 * This app declares the memory required by the module (state, input/output buffers)
 * and exercises each module’s init and one frame of processing
 * It is intended for measuring memory footprint,
 * not for functional verification.
 *
 * Select the module at compile time by defining exactly one of,
 * -DAEC, -DIC, -DVNR, -DAGC, -DNS, -DADEC, per app
 */

extern aec_task_distribution_t tdist;
void test_aec() {
    // Allocate signal data
    int32_t DWORD_ALIGNED frame_y[AEC_MAX_Y_CHANNELS][AEC_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED frame_x[AEC_MAX_X_CHANNELS][AEC_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED output[AEC_MAX_Y_CHANNELS][AEC_FRAME_ADVANCE];

    // Initialise AEC
    aec_state_t DWORD_ALIGNED aec_state;
    aec_init(&aec_state,
            AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS,
            AEC_MAIN_FILTER_PHASES, AEC_SHADOW_FILTER_PHASES, &tdist);

    aec_process_frame(&aec_state, output, NULL, NULL, frame_y, frame_x);
}

void test_ic() {
    ic_state_t DWORD_ALIGNED ic_state;
    int32_t frame_y[IC_FRAME_ADVANCE];
    int32_t frame_x[IC_FRAME_ADVANCE];
    int32_t output[IC_FRAME_ADVANCE];
    float_s32_t input_vnr_pred;
    ic_init(&ic_state);
    ic_process_frame(&ic_state, output, frame_y, frame_x, &input_vnr_pred, 0);
}

void test_vnr() {
    int32_t input[VNR_FRAME_ADVANCE];
    float_s32_t vnr_out;

    // Initialise VNR
    vnr_state_t vnr;
    vnr_state_init(&vnr);
    vnr_process_frame(&vnr, &vnr_out, input);
}

void test_agc()
{
    agc_state_t agc_state;
    int32_t input[AGC_FRAME_ADVANCE];
    int32_t output[AGC_FRAME_ADVANCE];
    agc_meta_data_t agc_md;
    agc_config_t agc_conf_asr = AGC_PROFILE_ASR;

    agc_init(&agc_state, &agc_conf_asr);
    agc_process_frame(&agc_state, output, input, &agc_md);
}

void test_ns()
{
    ns_state_t DWORD_ALIGNED ns_state;
    int32_t input[NS_FRAME_ADVANCE];
    int32_t output[NS_FRAME_ADVANCE];
    ns_init(&ns_state);
    ns_process_frame(&ns_state, output, input);
}

void test_adec()
{
    adec_config_t adec_config;
    adec_state_t DWORD_ALIGNED adec_state;
    adec_output_t adec_output;
    adec_input_t adec_input;

    adec_init(&adec_state, &adec_config);
    adec_estimate_delay(
            &adec_input.from_de,
            NULL,
            20
            );
    adec_process_frame(
            &adec_state,
            &adec_output,
            &adec_input
            );
}

/**
 * @brief Entry point: dispatch to selected module test.
 *
 * Exactly one module macro must be defined. The function runs the
 * corresponding test_* to trigger allocation and a single frame of work.
 */
int main()
{
#if AEC
    test_aec();
#endif
#if IC
    test_ic();
#endif
#if VNR
    test_vnr();
#endif
#if AGC
    test_agc();
#endif
#if NS
    test_ns();
#endif
#if ADEC
    test_adec();
#endif
    return 0;
}
