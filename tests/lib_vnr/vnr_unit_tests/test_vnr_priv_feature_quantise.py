# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr, BATCH_SIZE

def test_vnr_priv_feature_quantise(rng, quantise, dut_runner):
    input_words_per_frame = BATCH_SIZE + 1 # 96 mantissas and 1 exponent
    output_words_per_frame = BATCH_SIZE // 4 # 96 int8 values
    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 2048
    ref_output = np.empty(0, dtype=np.int8)
    for _ in range(test_frames):
        # By setting high=1 we enure no value is greater than 0 since max normalised output is 0
        data = rand_int32_arr(rng, BATCH_SIZE, max=1)
        exp = rng.integers(-31, 0) # exp
        input_data = np.append(input_data, exp)
        input_data = np.append(input_data, data)
        # Ref implementation
        this_patch = pvc.int32_to_double(data, exp)
        quant_patch = quantise(this_patch)
        ref_output = np.append(ref_output, quant_patch)

    op = dut_runner(input_data)
    dut_output = op.view(np.int8)

    np.testing.assert_allclose(dut_output, ref_output, rtol=0, atol=0)

    print("max_diff = ",np.max(np.abs(ref_output-dut_output)))
