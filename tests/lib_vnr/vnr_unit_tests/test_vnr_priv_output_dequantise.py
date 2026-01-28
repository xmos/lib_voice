# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
from test_utils import rand_int32_arr
import py_vs_c_utils as pvc

def test_vnr_priv_output_dequantise(rng, dequantise, dut_runner):

    input_words_per_frame = 1 # 1 int32 value out of which only the 1st byte is relevant since inference output is a single byte
    output_words_per_frame = 2 # 1 float_s32_t value
    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 2048
    ref_output_double = np.empty(0, dtype=np.float64)

    for _ in range(test_frames):
        data = rand_int32_arr(rng, 1, min=np.iinfo(np.int8).min, max=np.iinfo(np.int8).max + 1)
        input_data = np.append(input_data, data)

        # Reference dequantise implementation
        data = data.astype(dtype=np.int8)
        dequant_output = dequantise(data)
        ref_output_double = np.append(ref_output_double, dequant_output)

    op = dut_runner(input_data)

    dut_out = pvc.float_s32_arr_to_double(op)
    np.testing.assert_allclose(dut_out, ref_output_double, rtol=0, atol=0)

    max_diff = np.max(np.abs(dut_out - ref_output_double))
    print("max_diff = ", max_diff)
