# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr

def test_vnr_priv_mel_compute(rng, vnr_obj, dut_runner):

    fd_frame_len = int(vnr.NFFT/2 + 1)
    # No. of int32 values sent to dut as input per frame
    input_words_per_frame = fd_frame_len*2 + 1 #fd_frame_len complex values and 1 exponent per frame

    # No. of int32 output values expected from dut per frame (257 complex data values and 1 exponent)
    output_words_per_frame = vnr.MEL_FILTERS*2 #float_s32_t y[MEL_FILTERS]

    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 2048
    
    ref_output_float = np.empty(0, dtype=np.float64)
    for _ in range(test_frames):
        exp = rng.integers(-32, -8)
        data = rand_int32_arr(rng, fd_frame_len*2, 5)
        input_data = np.append(input_data, exp)
        input_data = np.append(input_data, data)

        # Ref Mel filtering implementation
        X_spect = pvc.int32_to_double(data, exp)
        X_spect = X_spect.astype(np.float64).view(np.complex128)

        out_spect = np.abs(X_spect)**2
        out_spect = np.dot(out_spect, vnr_obj.mel_fbank)

        ref_output_float = np.append(ref_output_float, out_spect)

    op = dut_runner(input_data)

    dut_output_float = pvc.float_s32_arr_to_double(op)

    np.testing.assert_allclose(dut_output_float, ref_output_float, rtol=1e-8, atol=0)

    print(f"Max diff = {np.max(np.abs(dut_output_float - ref_output_float))}")

