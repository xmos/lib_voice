// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <string.h>
#include <stdlib.h>

#include "pipeline_state.h"

extern aec_task_distribution_t tdist;
void pipeline_init(pipeline_state_t *state) {
    memset(state, 0, sizeof(pipeline_state_t));

    // Initialise AEC, DE, ADEC stages
    aec_conf_t aec_de_mode_conf, aec_non_de_mode_conf;

    aec_non_de_mode_conf.num_y_channels = AP_MAX_Y_CHANNELS;
    aec_non_de_mode_conf.num_x_channels = AP_MAX_X_CHANNELS;
    aec_non_de_mode_conf.num_main_filt_phases = AEC_MAIN_FILTER_PHASES;
    aec_non_de_mode_conf.num_shadow_filt_phases = AEC_SHADOW_FILTER_PHASES;
    aec_non_de_mode_conf.tdist = &tdist;

    aec_de_mode_conf.num_y_channels = 1;
    aec_de_mode_conf.num_x_channels = 1;
    aec_de_mode_conf.num_main_filt_phases = 30;
    aec_de_mode_conf.num_shadow_filt_phases = 0;
    aec_de_mode_conf.tdist = &tdist;

    // Disable ADEC's automatic mode. We only want to estimate and correct for the delay at startup
    adec_config_t adec_conf;
    adec_conf.bypass = 1; // Bypass automatic DE correction

    // Force a delay correction cycle, so that delay correction happens once after initialisation.
    // Make sure this is set back to 0 after adec has requested a transition into DE mode once,
    // to stop any further delay correction (automatic or forced) by ADEC
    adec_conf.force_de_cycle_trigger = 1;

    stage1_init(&state->stage_1_state, &aec_de_mode_conf, &aec_non_de_mode_conf, &adec_conf);

    // Initialise IC, VNR
    ic_init(&state->ic_state);

    // Initialise NS
    ns_init(&state->ns_state);

    // Initialise AGC
    agc_config_t agc_conf_asr = AGC_PROFILE_ASR;
    agc_init(&state->agc_state, &agc_conf_asr);
}

void pipeline_process_frame(pipeline_state_t *state,
        int32_t (*input_y_data)[AP_FRAME_ADVANCE],
        int32_t (*input_x_data)[AP_FRAME_ADVANCE],
        int32_t output_data[AP_FRAME_ADVANCE])
{
    pipeline_metadata_t md;
    memset(&md, 0, sizeof(pipeline_metadata_t));

    /** Stage1 - AEC, DE, ADEC*/
    // stage1 will not process the frame in-place,
    // since mic input is needed to overwrite the output in certain cases
    int32_t stage_1_out[AEC_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];

    stage1_process_frame(&state->stage_1_state, &stage_1_out[0], &md.max_ref_energy,
            &md.aec_corr_factor, &md.ref_active_flag, input_y_data, input_x_data);

    int32_t ic_output[AP_FRAME_ADVANCE];
    float_s32_t input_vnr_pred;

    ic_process_frame(&state->ic_state, stage_1_out[0], stage_1_out[1], ic_output, &input_vnr_pred);
    md.vnr_pred_flag = input_vnr_pred;

    /** NS*/
    int32_t ns_output[AP_FRAME_ADVANCE];

    ns_process_frame(&state->ns_state, ns_output, ic_output);

    /** AGC*/
    agc_meta_data_t agc_md;
    agc_md.aec_ref_power = md.max_ref_energy;
    agc_md.vnr_flag = md.vnr_pred_flag;
    agc_md.ref_active_flag = md.ref_active_flag;
    agc_md.aec_corr_factor = md.aec_corr_factor;

    agc_process_frame(&state->agc_state, output_data, ns_output, &agc_md);
}

