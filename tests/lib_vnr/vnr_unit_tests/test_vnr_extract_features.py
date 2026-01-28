# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import py_voice.modules.vnr as vnr
import py_vs_c_utils as pvc
from test_utils import rand_int32_arr, stft, BATCH_SIZE

def test_vnr_extract_features(rng, quantise, vnr_obj, dut_runner):

    input_words_per_frame = vnr.FRAME_ADVANCE + 1 # No. of int32 values sent to dut as input per frame

    norm_patch_output_len = BATCH_SIZE + 1 # + 1 for exponent
    quant_patch_output_len = BATCH_SIZE / 4 # / 4 because the output is int8 but we read in words
    output_subsets_len = [norm_patch_output_len, quant_patch_output_len]
    output_words_per_frame =  norm_patch_output_len +  quant_patch_output_len # Both normalised and quantised patches sent as output

    input_data = np.array([input_words_per_frame, output_words_per_frame], dtype=np.int32)

    test_frames = 1024
    ref_normalised_output = np.empty(0, dtype=np.float64)

    x_data = np.zeros(vnr.FRAME_LEN, dtype=np.float64)    
    for _ in range(test_frames):
        enable_highpass = rng.integers(2)

        data = rand_int32_arr(rng, vnr.FRAME_ADVANCE, 8)

        input_data = np.append(input_data, data)
        input_data = np.append(input_data, enable_highpass)

        # Ref form input frame implementation
        new_x_frame = pvc.int32_to_double(data, -31)
        X_spect, x_data = stft(x_data, new_x_frame, vnr.FRAME_LEN, vnr.FRAME_ADVANCE, vnr.NFFT)
        normalised_patch = vnr_obj.extract_features(X_spect, hp=enable_highpass)
        ref_normalised_output = np.append(ref_normalised_output, normalised_patch)

    ref_quantised_output = quantise(ref_normalised_output)

    op = dut_runner(input_data)
    # Deinterleave dut output into normalised and quantised patches
    # For that, repeat lens and run cumilative sum to get their indexes
    sections = np.cumsum(np.tile(output_subsets_len, test_frames))[:-1].astype(np.int32)
    op_split = np.split(op, sections)

    op_norm_patch = np.concatenate((op_split[0::2]))
    op_quant_patch = np.concatenate((op_split[1::2]))

    # recast to int8 with the same data
    dut_quantised_output = op_quant_patch.view(np.int8)
    dut_normalised_output = pvc.bfp_s32_arr_to_double(op_norm_patch, BATCH_SIZE, test_frames)

    # Compare normalised output
    dut = dut_normalised_output
    ref = ref_normalised_output

    np.testing.assert_allclose(dut, ref, rtol=0, atol=0.005)
    np.testing.assert_allclose(dut, ref, rtol=0.15, atol=0)

    arith_closeness, geo_closeness = pvc.get_closeness_metric(ref, dut)
    assert(geo_closeness > 0.999), f"ERROR: normalised_output geo_closeness below pass threshold"
    assert(arith_closeness > 0.999), f"ERROR: normalised_output arith_closeness below pass threshold"

    # Compare quantised output
    np.testing.assert_allclose(dut_quantised_output, ref_quantised_output, rtol=0, atol=1)

    diff = np.abs((dut_normalised_output - ref_normalised_output))
    percent_diff = np.abs((dut_normalised_output - ref_normalised_output)/(ref_normalised_output+np.finfo(float).eps))
    
    print(f"max diff normalised patch = {np.max(diff)}")
    print(f"max diff percent normalised patch = {np.max(percent_diff)*100}%")
    print(f"max diff quantised output = {np.max(np.abs(dut_quantised_output - ref_quantised_output))}")
