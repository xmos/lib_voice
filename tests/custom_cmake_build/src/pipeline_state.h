// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef AP_STAGE_A_STATE_H
#define AP_STAGE_A_STATE_H

#include "stage1.h"
#include "ic.h"
#include "ns.h"
#include "agc.h"
#include "pipeline_config.h"

typedef struct {
    float_s32_t max_ref_energy;
    float_s32_t aec_corr_factor;
    int32_t ref_active_flag;
    float_s32_t vnr_pred_flag;
}pipeline_metadata_t;

typedef struct {
    // Stage1 - AEC, DE, ADEC
    stage1_t DWORD_ALIGNED stage_1_state;
    // IC, VNR
    ic_state_t DWORD_ALIGNED ic_state;
    float_s32_t input_vnr_pred;
    // NS
    ns_state_t DWORD_ALIGNED ns_state;
    // AGC
    agc_state_t agc_state;
} pipeline_state_t;

#endif
