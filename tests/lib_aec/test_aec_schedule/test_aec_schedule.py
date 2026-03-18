# Copyright 2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
from pathlib import Path
import os
import soundfile as sf
import py_vs_c_utils as pvc
from run_dut import run_dut
import numpy as np

def test_aec_schedule(target):
    one_thread_xe = Path(__file__).parent / "bin" / "aec_std_arch_1thread" / "test_aec_schedule_aec_std_arch_1thread.xe"
    two_thread_xe = Path(__file__).parent / "bin" / "aec_std_arch_2threads" / "test_aec_schedule_aec_std_arch_2threads.xe"

    assert one_thread_xe.exists()
    assert two_thread_xe.exists()

    hydra_audio_path = Path(os.environ.get("hydra_audio_PATH", "~/hydra_audio")).expanduser()
    wav_name = hydra_audio_path / "adec_profile_test_stream" / "input_aec_delay_change_short.wav"
    input_data_float, _ = sf.read(wav_name)
    input_data_float = input_data_float.T # ch x samples format

    input_data = pvc.float_to_int32(input_data_float)
    assert input_data.ndim == 2
    assert input_data.shape[0] == 4
    input_data = pvc.interleave_channel_frames(input_data, 240)

    out1, _ = run_dut(input_data, one_thread_xe, target)
    out2, _ = run_dut(input_data, two_thread_xe, target)
    assert isinstance(out1, np.ndarray) and isinstance(out2, np.ndarray)
    assert out1.shape == out2.shape, "Output shapes differ"
    assert np.array_equal(out1, out2), "Outputs differ between 1-thread and 2-thread schedules"

if __name__ == "__main__":
    test_aec_schedule()
