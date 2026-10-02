// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include "stage1.h"

// aec_init() asserts the same conditions, but aec_de_mode_conf only reaches aec_init() when
// ADEC first triggers a delay estimation cycle
static inline void assert_aec_conf_supported(const aec_conf_t *conf)
{
    aec_assert_config_supported(conf->num_y_channels, conf->num_x_channels,
            conf->num_main_filt_phases, conf->num_shadow_filt_phases);
}

static inline void aec_switch_configuration(stage1_t *state, aec_conf_t *conf)
{
    aec_init(&state->aec_state,
            conf->num_y_channels, conf->num_x_channels,
            conf->num_main_filt_phases, conf->num_shadow_filt_phases, conf->tdist);
}

static inline void get_delayed_frame(
        int32_t (*input_y_data)[AEC_FRAME_ADVANCE],
        int32_t (*input_x_data)[AEC_FRAME_ADVANCE],
        delay_buf_state_t *delay_state)
{
    int num_channels = (delay_state->delay_samples) > 0 ? STAGE1_MAX_Y_CHANNELS : AEC_MAX_X_CHANNELS;
    if (delay_state->delay_samples >= 0) {/** Requested Mic delay +ve => delay mic*/
        for(int ch=0; ch<num_channels; ch++) {
            for(int i=0; i<AEC_FRAME_ADVANCE; i++) {
                get_delayed_sample(delay_state, &input_y_data[ch][i], ch);
            }
        }
    }
    else if (delay_state->delay_samples < 0) {/* Requested Mic delay negative => advance mic which can't be done, so delay reference*/
        for(int ch=0; ch<num_channels; ch++) {
            for(int i=0; i<AEC_FRAME_ADVANCE; i++) {
                get_delayed_sample(delay_state, &input_x_data[ch][i], ch);
            }
        }
    }
    return;
}

void stage1_init(stage1_t *state, aec_conf_t *de_conf, aec_conf_t *non_de_conf, adec_config_t *adec_config) {
    assert_aec_conf_supported(de_conf);
    assert_aec_conf_supported(non_de_conf);

    state->delay_estimator_enabled = 0;

    delay_buffer_init(&state->delay_state, 0/*Initialise with 0 delay_samples*/);
    memcpy(&state->aec_de_mode_conf, de_conf, sizeof(aec_conf_t));
    memcpy(&state->aec_non_de_mode_conf, non_de_conf, sizeof(aec_conf_t));

    adec_init(&state->adec_state, adec_config);
    aec_switch_configuration(state, &state->aec_non_de_mode_conf);
}

/** Process a frame of data through AEC and ADEC*/
void stage1_process_frame(stage1_t *state, int32_t (*output_frame)[AEC_FRAME_ADVANCE],
    float_s32_t *max_ref_energy, float_s32_t *aec_corr_factor, int32_t *ref_active_flag,
    int32_t (*input_y)[AEC_FRAME_ADVANCE], int32_t (*input_x)[AEC_FRAME_ADVANCE])
{
    delay_buf_state_t *delay_state_ptr = &state->delay_state;
    get_delayed_frame(
            input_y,
            input_x,
            delay_state_ptr
            );

    /** AEC*/
    aec_process_frame(&state->aec_state, output_frame, NULL, ref_active_flag, input_y, input_x);

    /** Update metadata*/
    *max_ref_energy = aec_calc_max_input_energy(input_x, state->aec_state.main_state.shared_state->num_x_channels);
    for(unsigned ch=0; ch<state->aec_state.main_state.shared_state->num_y_channels; ch++) {
        aec_corr_factor[ch] = aec_calc_corr_factor(&state->aec_state.main_state, ch);
    }

    /** Delay Estimation*/
    adec_input_t adec_in;
    adec_estimate_delay(
            &adec_in.from_de,
            state->aec_state.main_state.h_hat[0],
            state->aec_state.main_state.num_phases
            );


    /** ADEC*/
    // Create input to ADEC from AEC
    adec_in.from_aec.y_ema_energy_ch0 = state->aec_state.main_state.shared_state->y_ema_energy[0];
    adec_in.from_aec.error_ema_energy_ch0 = state->aec_state.main_state.error_ema_energy[0];
    adec_in.from_aec.shadow_flag_ch0 = state->aec_state.main_state.shared_state->shadow_filter_params.shadow_flag[0];
    adec_in.far_end_active_flag = state->aec_state.main_state.shared_state->ref_active_flag;

    adec_output_t adec_output;
    adec_process_frame(
            &state->adec_state,
            &adec_output,
            &adec_in
            );

    //** Reset AEC state if needed*/
    if(adec_output.reset_aec_flag) {
        aec_reset_state(&state->aec_state);
    }

    /** Update delay buffer if there's a delay change requested by ADEC*/
    if(adec_output.delay_change_request_flag == 1){
        // Update delay_buffer delay_samples with mic delay requested by adec
        update_delay_samples(&state->delay_state, adec_output.requested_mic_delay_samples);
        for(int ch=0; ch<STAGE1_MAX_Y_CHANNELS; ch++) {
            reset_partial_delay_buffer(&state->delay_state, ch);
        }
    }

    // Overwrite output with mic input if delay estimation enabled, otherwise pass through the mic channels the AEC
    // hasn't processed. The AEC cannot process the frame in-place because of this.
    int first_mic_ch = state->delay_estimator_enabled ? 0 : state->aec_state.main_state.shared_state->num_y_channels;
    for(int ch=first_mic_ch; ch<STAGE1_MAX_Y_CHANNELS; ch++) {
        vpu_memcpy(&output_frame[ch][0], &input_y[ch][0], AEC_FRAME_ADVANCE*sizeof(int32_t));
    }

    /** Switch AEC config if needed*/
    if (adec_output.delay_estimator_enabled_flag && !state->delay_estimator_enabled) {
        /** Now that a AEC -> DE change has been requested, reset force_de_cycle_trigger in case this transition is
         * requested as a result of force_de_cycle_trigger being set*/
        state->adec_state.adec_config.force_de_cycle_trigger = 0;

        // Initialise AEC for delay estimation config
        aec_switch_configuration(state, &state->aec_de_mode_conf);
        state->aec_state.main_state.shared_state->config_params.coh_mu_conf.adaption_config = AEC_ADAPTION_FORCE_ON;
        state->delay_estimator_enabled = 1;
    } else if ((!adec_output.delay_estimator_enabled_flag && state->delay_estimator_enabled)) {
        // Start AEC for normal aec config
        aec_switch_configuration(state, &state->aec_non_de_mode_conf);
        state->delay_estimator_enabled = 0;
    }
}
