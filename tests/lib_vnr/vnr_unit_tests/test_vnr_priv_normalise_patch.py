# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr, BATCH_SIZE

def test_vnr_priv_normalise_patch(rng, vnr_obj, dut_runner):

    input_words_per_frame = vnr.MEL_FILTERS #No. of int32 values sent to dut as input per frame
    output_words_per_frame = BATCH_SIZE + 1

    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 2048

    ref_output_float = np.empty(0, dtype=np.float64)
    for _ in range(test_frames):
        # Generate input data
        data = rand_int32_arr(rng, vnr.MEL_FILTERS, 8)
        input_data = np.append(input_data, data)

        # Ref form input frame implementation
        ref_new_slice = pvc.int32_to_double(data, -24)
        vnr_obj.add_new_slice(ref_new_slice, buffer_number=0)
        normalised_patch = vnr_obj.normalise_patch(vnr_obj.feature_buffers[0])
        ref_output_float = np.append(ref_output_float, normalised_patch)

    op = dut_runner(input_data)

    dut_out = pvc.bfp_s32_arr_to_double(op, BATCH_SIZE, test_frames)
    # allow 1 bit difference
    np.testing.assert_allclose(dut_out, ref_output_float, rtol=0, atol=2**-23)

    max_diff = np.max(np.abs(dut_out - ref_output_float))
    print("max_diff = ",max_diff)
