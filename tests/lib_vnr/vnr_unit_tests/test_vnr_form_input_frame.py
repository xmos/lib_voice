# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr, stft

def test_vnr_form_input_frame(rng, dut_runner):
    input_words_per_frame = vnr.FRAME_ADVANCE #No. of int32 values sent to dut as input per frame

    fd_frame_len = int(vnr.NFFT//2 + 1)
    output_words_per_frame = fd_frame_len*2 + 1 # No. of int32 output values expected from dut per frame (257 complex data values and 1 exponent)

    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 2048

    x_data = np.zeros(vnr.FRAME_LEN, dtype=np.float64)    
    ref_output = np.empty(0, dtype=np.complex128)
    for _ in range(test_frames):
        # Generate input data
        data = rand_int32_arr(rng, vnr.FRAME_ADVANCE, 8)
        input_data = np.append(input_data, data)

        # Ref form input frame implementation
        new_x_frame = pvc.int32_to_double(data, -31)
        X_spect, x_data = stft(x_data, new_x_frame, vnr.FRAME_LEN, vnr.FRAME_ADVANCE, vnr.NFFT)
        ref_output = np.append(ref_output, X_spect).astype(np.complex128)

    op = dut_runner(input_data) # dut data has exponent followed by 257*2 data values

    dut_out = pvc.bfp_s32_arr_to_double(op, fd_frame_len*2, test_frames)
    ref_out = ref_output.view(np.float64)

    np.testing.assert_allclose(dut_out, ref_out, rtol=1e-2, atol=0)
