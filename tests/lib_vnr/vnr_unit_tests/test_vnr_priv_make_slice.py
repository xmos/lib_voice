# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr, stft

def test_vnr_priv_make_slice(rng, vnr_obj, dut_runner):

    input_words_per_frame = vnr.FRAME_ADVANCE + 1 #No. of int32 values sent to dut as input per frame
    output_words_per_frame = vnr.MEL_FILTERS # MEL_FILTERS uq8_24 values. Exponent fixed to -24

    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)    

    test_frames = 2048

    x_data = np.zeros(vnr.FRAME_LEN, dtype=np.float64)    
    ref_output_float = np.empty(0, dtype=np.float64)

    for _ in range(test_frames):
        enable_highpass = rng.integers(2)
        # Generate input data
        data = rand_int32_arr(rng, vnr.FRAME_ADVANCE, 8)
        input_data = np.append(input_data, data)
        input_data = np.append(input_data, enable_highpass)

        # Ref form input frame implementation
        new_x_frame = pvc.int32_to_double(data, -31)
        X_spect, x_data = stft(x_data, new_x_frame, vnr.FRAME_LEN, vnr.FRAME_ADVANCE, vnr.NFFT)
        new_slice = vnr_obj.make_slice(X_spect, enable_highpass)

        ref_output_float = np.append(ref_output_float, new_slice)
        
    op = dut_runner(input_data)
    dut_mant = op.astype(np.float64)
    dut_exp = -24 # dut output is always 8.24

    dut_output_float = pvc.int32_to_double(dut_mant, dut_exp)

    np.testing.assert_allclose(dut_output_float, ref_output_float, rtol=0, atol=0.005)
    np.testing.assert_allclose(dut_output_float, ref_output_float, rtol=0.05, atol=0)

    percent_diff = np.abs((dut_output_float - ref_output_float)/ref_output_float)
    print(f"max make_slice output diff = {np.max(np.abs(dut_output_float - ref_output_float))}")
    print(f"max diff percent = {np.max(percent_diff)*100}%")
