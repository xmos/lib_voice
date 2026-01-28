# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr

def test_vnr_priv_log2(rng, dut_runner):

    # No. of int32 values sent to dut as input per frame
    input_words_per_frame = vnr.MEL_FILTERS*2 # MEL_FILTERS float_s32_t values 

    # No. of int32 output values expected from dut per frame
    output_words_per_frame = vnr.MEL_FILTERS # MEL_FILTERS uq8_24 values. Exponent fixed to -24

    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)
    test_frames = 2048

    ref_output_float = np.empty(0, dtype=np.float64)

    for _ in range(test_frames):
        data = np.zeros(vnr.MEL_FILTERS*2, dtype=np.int32)
        data[0::2] = rand_int32_arr(rng, vnr.MEL_FILTERS, 5, min=1)
        data[1::2] = rand_int32_arr(rng, vnr.MEL_FILTERS, min=-32, max=16) # exp
        data = np.array(data, dtype=np.int32)
        input_data = np.append(input_data, data)

        # Ref log2 implementation
        ref = pvc.float_s32_arr_to_double(data)
        y = np.log2(ref)
        ref_output_float = np.append(ref_output_float, y)

    op = dut_runner(input_data)
    dut_mant = op.astype(np.float64)
    dut_exp = -24 # dut output is always 8.24

    dut_output_float = pvc.int32_to_double(dut_mant, dut_exp)

    np.testing.assert_allclose(dut_output_float, ref_output_float, rtol=0, atol=0.005)
    np.testing.assert_allclose(dut_output_float, ref_output_float, rtol=0.05, atol=0)

    percent_diff = np.abs((dut_output_float - ref_output_float)/ref_output_float)
    print(f"max log2 output diff = {np.max(np.abs(dut_output_float - ref_output_float))}")
    print("max diff percent = ",np.max(percent_diff)*100)

