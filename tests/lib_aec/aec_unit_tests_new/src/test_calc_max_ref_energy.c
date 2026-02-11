// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <string.h>
#include <assert.h>

#include "aec.h"

#define NUM_CHANNELS (4)

void test_init() {}

void test(int32_t *output, int32_t *input)
{
    // const int32_t (*input_data)[AEC_FRAME_ADVANCE] = (const int32_t (*)[AEC_FRAME_ADVANCE]) &input[0];

    // float_s32_t max_energy = aec_calc_max_input_energy(input_data, NUM_CHANNELS);
    float_s32_t max_energy = aec_calc_max_input_energy(input, NUM_CHANNELS);
    memcpy(output, &max_energy, sizeof(float_s32_t));
}
