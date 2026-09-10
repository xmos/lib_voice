// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <string.h>
#include <stdlib.h>

#include "pipeline_state.h"

// Task distribution generated from this build config's AEC_SCHEDULE_CONFIG_<config>
extern aec_task_distribution_t tdist;

void pipeline_thread0_init(pipeline_state_thread0_t *state) {
    memset(state, 0, sizeof(pipeline_state_thread0_t));

    // Initialise AEC, DE, ADEC stages
    aec_conf_t aec_de_mode_conf, aec_non_de_mode_conf;
    // Non DE mode runs the AEC this build was configured for, so both build configs read the same
    // compile time values here. The difference between std arch and alt arch is in the AEC schedule
    // config the build supplies (see CMakeLists.txt), not in this code: alt arch is built for 1 y
    // channel and 15 main filter phases, std arch for 2 y channels and 10.
    aec_non_de_mode_conf.num_y_channels = AEC_MAX_Y_CHANNELS;
    aec_non_de_mode_conf.num_x_channels = AEC_MAX_X_CHANNELS;
    aec_non_de_mode_conf.num_main_filt_phases = AEC_MAIN_FILTER_PHASES;
    aec_non_de_mode_conf.num_shadow_filt_phases = AEC_SHADOW_FILTER_PHASES;
    aec_non_de_mode_conf.tdist = &tdist;

    aec_de_mode_conf.num_y_channels = ADEC_DE_MODE_Y_CHANNELS;
    aec_de_mode_conf.num_x_channels = ADEC_DE_MODE_X_CHANNELS;
    aec_de_mode_conf.num_main_filt_phases = ADEC_DE_MODE_MAIN_FILTER_PHASES;
    aec_de_mode_conf.num_shadow_filt_phases = ADEC_DE_MODE_SHADOW_FILTER_PHASES;
    aec_de_mode_conf.tdist = &tdist;

    // Disable ADEC's automatic mode. We only want to estimate and correct for the delay at startup
    adec_config_t adec_conf;
    adec_conf.bypass = 1; // Bypass automatic DE correction
#if DISABLE_INITIAL_DELAY_EST
    // Do not force a DE correction cycle on startup
    adec_conf.force_de_cycle_trigger = 0;
#else
    // Force a delay correction cycle, so that delay correction happens once after initialisation.
    // Make sure this is set back to 0 after adec has requested a transition into DE mode once,
    // to stop any further delay correction (automatic or forced) by ADEC
    adec_conf.force_de_cycle_trigger = 1;
#endif
    stage1_init(&state->stage_1_state, &aec_de_mode_conf, &aec_non_de_mode_conf, &adec_conf);
}

void pipeline_thread1_init(pipeline_state_thread1_t *state) {
    memset(state, 0, sizeof(pipeline_state_thread1_t));

    // Initialise IC, VNR
    ic_init(&state->ic_state);

    // Initialise NS
    ns_init(&state->ns_state);

    // Initialise AGC
    agc_config_t agc_conf_asr = AGC_PROFILE_ASR;
    agc_init(&state->agc_state, &agc_conf_asr);
}

void pipeline_process_frame_thread0(pipeline_state_thread0_t *state,
        int32_t (*input_y_data)[AP_FRAME_ADVANCE],
        int32_t (*input_x_data)[AP_FRAME_ADVANCE],
        int32_t (*output_data)[AP_FRAME_ADVANCE],
        pipeline_metadata_t *md_output)
{
    pipeline_metadata_t md;
    memset(&md, 0, sizeof(pipeline_metadata_t));

    /** Stage1 - AEC, DE, ADEC*/
    // stage1 will not process the frame in-place,
    // since mic input is needed to overwrite the output in certain cases
    int32_t stage_1_out[AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];

    stage1_process_frame(&state->stage_1_state, &stage_1_out[0], &md.max_ref_energy,
            &md.aec_corr_factor[0], &md.ref_active_flag, input_y_data, input_x_data);

    memcpy(&output_data[0][0], &stage_1_out[0][0], AP_MAX_Y_CHANNELS*AP_FRAME_ADVANCE*sizeof(int32_t));
    memcpy(md_output, &md, sizeof(pipeline_metadata_t));
}

void pipeline_process_frame_thread1(pipeline_state_thread1_t *state, pipeline_metadata_t *md_input,
        int32_t (*input_data)[AP_FRAME_ADVANCE],
        int32_t output_data[AP_FRAME_ADVANCE])
{
    pipeline_metadata_t md;
    memcpy(&md, md_input, sizeof(pipeline_metadata_t));

    // Bypass IC if the reference is high in the alt arch mode
#if ALT_ARCH_MODE
    if(md.ref_active_flag) {
        state->ic_state.config_params.bypass = 1;
    }
    else {
        state->ic_state.config_params.bypass = 0;
    }
#endif
    /** IC and VNR*/
    int32_t ic_output[AP_FRAME_ADVANCE];
    float_s32_t input_vnr_pred;

    ic_process_frame(&state->ic_state, input_data[0], input_data[1], ic_output, &input_vnr_pred);
    md.vnr_pred_flag = input_vnr_pred;

    /** NS*/
    int32_t ns_output[AP_FRAME_ADVANCE];

    ns_process_frame(&state->ns_state, ns_output, ic_output);

    /** AGC*/
    agc_meta_data_t agc_md;
    agc_md.aec_ref_power = md.max_ref_energy;
    agc_md.vnr_flag = md.vnr_pred_flag;
    agc_md.ref_active_flag = md.ref_active_flag;
    agc_md.aec_corr_factor = md.aec_corr_factor[0];

    agc_process_frame(&state->agc_state, output_data, ns_output, &agc_md);
}

