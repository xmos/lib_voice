// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "aec.h"
#include "aec_priv.h"

#define TEST_FRAME_LEN (AEC_PROC_FRAME_LENGTH / 2 + 1)

static aec_state_t aec_state;
void test_init()
{
    #if BUILD_NATIVE
    aec_task_distribution_t tdist = aec_tdist_chans2_threads1;
    #else
    aec_task_distribution_t tdist = aec_tdist_chans2_threads2;
    #endif

    aec_init(&aec_state, 1, 1, 10, 0, &tdist);
}

void test(int32_t *output, int32_t *input)
{
    bfp_complex_s32_t data;
    float_s32_t energy;
    bfp_complex_s32_init(&data, (int32_t *)&input[1], input[0], TEST_FRAME_LEN, 1);

    aec_calc_freq_domain_energy(&energy, &data);

    memcpy(output, &energy, sizeof(float_s32_t));
}
