# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import soundfile as sf
from pathlib import Path
import numpy as np
from run_dut import run_dut
from profile_xcore import parse_profile_log
from audio_generation import get_band_limited_noise

SAMPLE_RATE = 16000
ns_xe = Path(__file__).parent / "bin" / "test_ns_profile"
ns_src_folder = Path(__file__).parent / "src"
ns_src_folder = str(ns_src_folder)

def run_ns_xe(ns_xe, audio_in, audio_out, run_native, profile=False):

    input_data, _ = sf.read(audio_in, dtype=np.int32)

    assert len(input_data.shape) == 1, "Input data can be single channel only"

    local_exe = ns_xe
    if not run_native: local_exe = local_exe.with_suffix(".xe")

    output_data, xcore_stdo = run_dut(input_data, local_exe)

    sf.write(audio_out, output_data, SAMPLE_RATE)

    if not run_native and profile:
        parse_profile_log(
            xcore_stdo,
            ns_src_folder,
            worst_case_file="ns_prof.log",
            exclude_init=True
        )

def generate_test_audio(max_freq = SAMPLE_RATE // 2, db=-20):
    SAMPLE_COUNT = 2400

    noise = get_band_limited_noise(0, max_freq, samples=SAMPLE_COUNT, db=db, sample_rate=SAMPLE_RATE)

    sf.write("input.wav", noise, SAMPLE_RATE)

def test_ns_profile():
    generate_test_audio()
    run_ns_xe(ns_xe, "input.wav", "output.wav", False, True)

if __name__ == "__main__":
    generate_test_audio()
    run_ns_xe(ns_xe, "input.wav", "output.wav", True, True)
