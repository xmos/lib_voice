# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
from audio_generation import get_band_limited_noise
import time
import numpy as np
import argparse
from pathlib import Path
import soundfile as sf
from py_voice.modules.ns import ns
import shutil
from run_dut import run_dut
import py_vs_c_utils as pvc
import py_voice

c_ns_xe_path = Path(__file__).parents[1] / "test_ns_profile" / "bin" / "test_ns_profile"
PY_VOICE_ROOT = Path(py_voice.__file__).resolve().parent
ns_conf_path = PY_VOICE_ROOT / "config" / "components" / "ns_only.json"


SAMPLE_RATE = 16000
SAMPLE_COUNT = 160080


def generate_test_audio(max_freq, db=-20, samples=SAMPLE_COUNT):
    noise = get_band_limited_noise(0, max_freq, samples=samples, db=db, sample_rate=SAMPLE_RATE)
    return noise

def process_xe(input_data, ns_xe, run_native = False):

    local_exe = ns_xe
    if not run_native: local_exe = local_exe.with_suffix(".xe")

    input_data = pvc.float_to_int32(input_data)

    output_data, _ = run_dut(input_data, local_exe)

    return pvc.int32_to_float(output_data)

def process_py(input_data):
    ns_obj = ns(ns_conf_path)
    # py_voice always expects 2d data
    input_data = np.reshape(input_data, (1, len(input_data)))
    output_data, _ = ns_obj.process_array(input_data)
    return np.reshape(output_data, output_data.shape[1])

def get_attenuation(in_data, out_data):

    # Calculate EWM of audio power in 1s window
    in_power = np.power(in_data, 2)
    out_power = np.power(out_data, 2)

    attenuation = []

    for i in range(len(in_power) // SAMPLE_RATE):
        window_start = i * SAMPLE_RATE
        window_end = window_start + SAMPLE_RATE
        av_in_power = np.mean(in_power[window_start:window_end])
        av_out_power = np.mean(out_power[window_start:window_end])
        new_atten = 10 * np.log10(av_in_power / av_out_power)
        attenuation.append(new_atten)

    return attenuation


def get_attenuation_c_py(test_id, noise_band, noise_db):
    input_file = "input.wav"

    output_file_c = "output_c.wav"
    output_file_py = "output_py.wav"

    audio_dir = Path(__file__).parent / test_id
    audio_dir.mkdir(exist_ok=True)
    input_data = generate_test_audio(noise_band, db=noise_db)
    sf.write(audio_dir / input_file, input_data, SAMPLE_RATE)

    out_c = process_xe(input_data, c_ns_xe_path)
    sf.write(audio_dir / output_file_c, out_c, SAMPLE_RATE)
    out_py = process_py(input_data)
    sf.write(audio_dir / output_file_py, out_py, SAMPLE_RATE)

    attenuation_c = get_attenuation(input_data, out_c)
    attenuation_py = get_attenuation(input_data, out_py)

    print("     C NS: {}".format(["%.2f"%item for item in attenuation_c]))
    print("    PY NS: {}".format(["%.2f"%item for item in attenuation_py]))

    shutil.rmtree(audio_dir)

    return attenuation_c, attenuation_py


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("noise_band", nargs='?', default=8000, type=int, help="Noise freq bandwidth")
    parser.add_argument("noise_level", nargs='?',default=-20, type=int, help="Nominal noise level (dBFS)")

    parser.parse_args()
    args = parser.parse_args()
    return args


def main():
    start_time = time.time()
    args = parse_arguments()
    get_attenuation_c_py("test", args.noise_band, args.noise_level)
    print(("--- {0:.2f} seconds ---" .format(time.time() - start_time)))


if __name__ == "__main__":
    main()
