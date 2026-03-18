# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr, BATCH_SIZE

def test_vnr_inference(rng, vnr_obj, dut_runner):

    input_words_per_frame = BATCH_SIZE + 1 # 96 mantissas and 1 exponent
    output_words_per_frame = 2
    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 4096
    ref_output_double = np.empty(0, dtype=np.float64)

    for _ in range(test_frames):
        data = rand_int32_arr(rng, BATCH_SIZE, max=0)
        exp = rng.integers(-31, 0) # exp
        input_data = np.append(input_data, exp)
        input_data = np.append(input_data, data)

        # Ref implementation
        this_patch = pvc.int32_to_double(data, exp)
        this_patch = this_patch.reshape(1, 1, vnr.PATCH_WIDTH, vnr.MEL_FILTERS)
        ref_output_double = np.append(ref_output_double, vnr_obj.run(this_patch))

    op = dut_runner(input_data)

    dut_output_double = pvc.float_s32_arr_to_double(op)

    np.testing.assert_allclose(dut_output_double, ref_output_double, rtol=0, atol=0.05)

    print("max_diff = ",np.max(np.abs(ref_output_double - dut_output_double)))
    arith_closeness, geo_closeness = pvc.get_closeness_metric(ref_output_double, dut_output_double)
    print(f"arith_closeness = {arith_closeness}, geo_closeness = {geo_closeness}")
    assert(geo_closeness > 0.98), "inference output geo_closeness below pass threshold"
    assert(arith_closeness > 0.94), "inference output arith_closeness below pass threshold"
