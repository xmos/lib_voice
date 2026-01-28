# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import scipy.signal as spsig
from pathlib import Path
import soundfile as sf
from run_dut import run_dut
import py_vs_c_utils as pvc
from profile_xcore import parse_profile_log

ic_src_folder = Path(__file__).parent / "src"
ic_src_folder = str(ic_src_folder)
ic_xe = Path(__file__).parent / "bin" / "test_ic_profile"
SAMPLE_RATE = 16000
FRAME_ADVANCE = 240

def run_ic_xe(ic_xe, audio_in, audio_out, target, profile):

    input_data, _ = sf.read(audio_in, dtype=np.int32)

    assert input_data.ndim == 2
    assert input_data.shape[1] == 2

    input_data = input_data.T

    input_data = pvc.interleave_channel_frames(input_data, FRAME_ADVANCE)

    output_data, xcore_stdo = run_dut(input_data, ic_xe, target)

    sf.write(audio_out, output_data, SAMPLE_RATE)

    if target != "native" and profile:
        parse_profile_log(
            xcore_stdo,
            ic_src_folder,
            worst_case_file="ic_prof.log",
            exclude_init=True
        )

def make_impulse(RT, t=None, fs=None):
    scale = 0.005
    scale_noise = 0.00005
    a = 3.0 * np.log(10.0) / RT
    if t is None:
        t = np.arange(2.0*RT*fs) / fs
    N = t.shape[0]
    h = np.zeros(N)
    e = np.exp(-a*t)
    reflections = N // 100
    reflection_index = np.random.randint(N, size=reflections)
    for n, idx in enumerate(reflection_index):
        if n % 2 == 0:
            flip = 1
        else:
            flip = -1
        h[idx] = flip * scale * t[idx] * e[idx]
    h += scale_noise * np.random.randn(t.shape[0]) * e
    return h

def create_wav_input():
    N = SAMPLE_RATE * 10
    np.random.seed(500)

    phases = 10
    fN = phases * 240

    # build impulse response
    RT = 0.15
    h = make_impulse(RT, fs=SAMPLE_RATE)
    h = h/h.max()
    hN = len(h)

    u = np.random.randn(N)

    d = spsig.convolve(u, h, 'full')[:N]
    if fN > hN:
        d = d[hN-1:hN-fN]
    else:
        d = d[hN-1:]

    sig_level = 0.01  #20dB attenuation
    d = d * sig_level
    u = u * sig_level

    in_data = np.stack((d, u[hN-1:N]), axis=0)
    # crop to have full frames
    inx = in_data.shape[1] // FRAME_ADVANCE * FRAME_ADVANCE
    in_data = in_data[:, :inx]
    sf.write("input.wav", in_data.T, SAMPLE_RATE)

def test_ic_profile():
    create_wav_input()
    run_ic_xe(ic_xe, "input.wav", "output.wav", "xs3a", True)

if __name__ == "__main__":
    create_wav_input()
    run_ic_xe(ic_xe, "input.wav", "output.wav", "native", False)
