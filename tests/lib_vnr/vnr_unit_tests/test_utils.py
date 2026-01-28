# Copyright 2025-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import numpy as np
import py_voice.modules.vnr as vnr
from xmos_ai_tools.xinterpreters import TFLMHostInterpreter

BATCH_SIZE = vnr.PATCH_WIDTH * vnr.MEL_FILTERS

def rand_int32_arr(rng, size=None, hr_max=1, min=np.iinfo(np.int32).min, max=np.iinfo(np.int32).max+1):
    hr = rng.integers(hr_max)
    data = rng.integers(min, max, size=size, dtype=np.int32)
    data >>= hr
    return data

def stft(x_data, new_x_frame, x_data_len, new_x_frame_len, nfft):
    x_data = np.roll(x_data, -new_x_frame_len, axis=0)
    x_data[x_data_len - new_x_frame_len:] = new_x_frame
    X_spect = np.fft.rfft(x_data, nfft)
    return X_spect, x_data

def get_model_details(model_file):
    with TFLMHostInterpreter() as interpreter_tflite: # important to close interpreter to avoid OOM error
        interpreter_tflite.set_model(model_path=model_file)
        input_details = interpreter_tflite.get_input_details()[0]
        output_details = interpreter_tflite.get_output_details()[0]
    assert(input_details["dtype"] in [np.int8, np.uint8]), "Error: Need 8bit model for quantisation"
    assert(output_details["dtype"] in [np.int8, np.uint8]), "Error: Need 8bit model for quantisation"
    return input_details, output_details

def quantise_patch(this_patch, input_details):
    input_scale, input_zero_point = input_details["quantization"]
    this_patch = this_patch / input_scale + input_zero_point
    this_patch = np.round(this_patch)
    this_patch = np.clip(this_patch, np.iinfo(input_details["dtype"]).min, np.iinfo(input_details["dtype"]).max)
    this_patch = this_patch.astype(input_details["dtype"])
    return this_patch

def dequantise_output(output_data, output_details):
    output_scale, output_zero_point = output_details["quantization"]
    output_data = output_data.astype(np.float64)
    output_data = (output_data - output_zero_point) * output_scale
    return output_data
