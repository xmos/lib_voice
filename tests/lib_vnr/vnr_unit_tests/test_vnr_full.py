# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr, stft

def test_vnr_full(rng, vnr_obj, dut_runner):

    input_words_per_frame = vnr.FRAME_ADVANCE + 1#No. of int32 values sent to dut as input per frame
    output_words_per_frame = 2

    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 2048
    ref_output_double = np.empty(0, dtype=np.float64)

    x_data = np.zeros(vnr.FRAME_LEN, dtype=np.float64)    

    for _ in range(test_frames):
        enable_highpass = rng.integers(2)
        # Generate input data
        data = rand_int32_arr(rng, vnr.FRAME_ADVANCE, 8)
        input_data = np.append(input_data, data)
        input_data = np.append(input_data, enable_highpass)

        # Ref VNR implementation
        new_x_frame = pvc.int32_to_double(data, -31)
        X_spect, x_data = stft(x_data, new_x_frame, vnr.FRAME_LEN, vnr.FRAME_ADVANCE, vnr.NFFT)
        this_patch = vnr_obj.extract_features(X_spect, hp=enable_highpass)
        ref_output_double = np.append(ref_output_double, vnr_obj.run(this_patch))

    op = dut_runner(input_data)

    dut_output_double = pvc.float_s32_arr_to_double(op)

    np.testing.assert_allclose(dut_output_double, ref_output_double, rtol=0, atol=0.05)

    print("max_diff = ",np.max(np.abs(ref_output_double - dut_output_double)))
    arith_closeness, geo_closeness = pvc.get_closeness_metric(ref_output_double, dut_output_double)
    print(f"arith_closeness = {arith_closeness}, geo_closeness = {geo_closeness}")
    assert(geo_closeness > 0.97), "inference output geo_closeness below pass threshold"
    assert(arith_closeness > 0.95), "inference output arith_closeness below pass threshold"
