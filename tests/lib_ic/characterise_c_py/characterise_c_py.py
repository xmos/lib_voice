# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import time
import numpy as np
import argparse
import pyroomacoustics as pra
import soundfile as sf
from pathlib import Path

from audio_generation import get_band_limited_noise
from py_voice.modules import ic
from py_voice.config import config
from run_dut import run_dut
import py_vs_c_utils as pvc

NOISE_FLOOR_dBFS = -63.0
SIGMA2_AWGN = ((10 ** (float(NOISE_FLOOR_dBFS)/20)) * np.iinfo(np.int32).max) ** 2

SAMPLE_RATE = 16000
SAMPLE_COUNT = 160080
FRAME_ADVANCE = 240

MIN_NOISE_FREQ = 0

ROOM_X = 4.0
ROOM_Y = 4.0
ROOM_Z = 2.0

MIC_X_POINT = ROOM_X / 2 - 0.1
MIC_Y_POINT = ROOM_Y / 2 + 0.1
MIC_Z_POINT = ROOM_Z / 2

MIC_SPACING = 0.072
MIC_0_X = MIC_X_POINT - MIC_SPACING / 2
MIC_1_X = MIC_X_POINT + MIC_SPACING / 2

NOISE_DISTANCE = 1.5

IC_XE = Path(__file__).parent / "bin" / "test_ic_characterise_c_py"
audio_dir = Path(__file__).parent / "pytest_audio"
ap_config_file = Path(__file__).parents[2] / "shared" / "config" / "ic_conf_big_delta.json"
ap_conf = config.get_config_dict(ap_config_file)

# Use Sabine's Eq to calc average absorption factor of room surfaces
def get_absorption(x, y, z, rt60):
    V = x * y * z
    S = (2 * x * y) + (2 * x * z) + (2 * y * z)
    absorption = 0.1611 * V/(S * rt60)
    return absorption

def generate_test_audio(max_freq, db, angle_theta, rt60, samples=SAMPLE_COUNT):

    noise = get_band_limited_noise(MIN_NOISE_FREQ, max_freq, samples=samples, db=db, sample_rate=SAMPLE_RATE)
    audio_anechoic = np.asarray(noise * np.iinfo(np.int32).max, dtype=np.int32)

    noise_x = MIC_X_POINT + (NOISE_DISTANCE * np.cos(angle_theta))
    noise_y = MIC_Y_POINT + (NOISE_DISTANCE * np.sin(angle_theta))
    room_dim = [ROOM_X, ROOM_Y, ROOM_Z]

    if noise_x > ROOM_X or noise_x < 0 or noise_y > ROOM_Y or noise_y < 0:
        raise Exception("Speech location (%.2r, %.2r) outside room dimensions (%r, %r)"%(noise_x, noise_y, ROOM_X, ROOM_Y))

    absorption = get_absorption(ROOM_X, ROOM_Y, ROOM_Z, rt60)
    shoebox = pra.ShoeBox(room_dim, absorption=absorption, fs=SAMPLE_RATE, max_order=15, sigma2_awgn=SIGMA2_AWGN)
    shoebox.add_source([noise_x, noise_y, 1], signal=audio_anechoic)
    mics = np.array([[MIC_0_X, MIC_Y_POINT, 1], [MIC_1_X, MIC_Y_POINT, 1]]).T
    shoebox.add_microphone_array(pra.MicrophoneArray(mics, shoebox.fs))
    shoebox.simulate()

    mic_output = shoebox.mic_array.signals
    output = np.array(mic_output, dtype=np.int32)
    # crop to have full frames
    inx = output.shape[1] // FRAME_ADVANCE * FRAME_ADVANCE
    output = output[:, :inx]
    return pvc.int32_to_float(output)

def process_py(input_data):

    ic_obj = ic.ic(ap_conf)
    output_data, _ = ic_obj.process_array(input_data)
    return np.reshape(output_data, output_data.shape[1])

def process_c(input_data):

    assert input_data.ndim == 2
    assert input_data.shape[0] == 2

    input_data = pvc.float_to_int32(input_data)

    input_data = pvc.interleave_channel_frames(input_data, FRAME_ADVANCE)

    output_data, _ = run_dut(input_data, IC_XE, "xs3a")

    return pvc.int32_to_float(output_data)

def get_attenuation(in_data, out_data):
    # Calculate EWM of audio power in 1s window
    in_power = np.power(in_data[0, :], 2)
    out_power = np.power(out_data, 2)

    assert in_power.shape == out_power.shape
    assert in_power.ndim == out_power.ndim == 1
    attenuation = []

    for i in range(len(in_power) // SAMPLE_RATE):
        window_start = i*SAMPLE_RATE
        window_end = window_start + SAMPLE_RATE

        av_in_power = np.mean(in_power[window_start:window_end])

        av_out_power = np.mean(out_power[window_start:window_end])
        new_atten = 10 * np.log10(av_in_power / av_out_power) if av_out_power != 0 else 1000
        attenuation.append(new_atten)

    return attenuation

def get_attenuation_c_py(test_id, noise_band, noise_db, angle, rt60):
    audio_dir.mkdir(exist_ok=True)
    test_name = f"{test_id}_{angle}"
    input_file = audio_dir / f"in_{test_name}.wav"
    output_file_py = audio_dir / f"out_{test_name}_py.wav"
    output_file_c = audio_dir / f"out_{test_name}_c.wav"

    angle_theta = angle * np.pi/180
    input_data = generate_test_audio(noise_band, noise_db, angle_theta, rt60)
    sf.write(input_file, input_data.T, SAMPLE_RATE)

    out_py = process_py(input_data)
    sf.write(output_file_py, out_py, SAMPLE_RATE)
    out_c = process_c(input_data)
    sf.write(output_file_c, out_c, SAMPLE_RATE)

    attenuation_py = get_attenuation(input_data, out_py)
    attenuation_c = get_attenuation(input_data, out_c)

    print("PYTHON SUP: {}".format(["%.2f"%item for item in attenuation_py]))
    print("     C SUP: {}".format(["%.2f"%item for item in attenuation_c]))

    return attenuation_c, attenuation_py


def angle_type(x):
    x = int(x)
    if x > 180 or x < 0:
        raise argparse.ArgumentTypeError("%r not in range [0, 180]"%(x,))
    return x


def rt60_type(x):
    x = float(x)
    if x <= 0.0:
        raise argparse.ArgumentTypeError("%r not greater than 0"%(x,))
    return x


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--angle", nargs='?', default=90, type=angle_type, help="Angular position of noise source")
    parser.add_argument("--rt60", nargs='?', default=90, type=rt60_type, help="RT60 of environment")
    parser.add_argument("--noise_band", nargs='?', default=8000, type=int, help="Noise freq bandwidth")
    parser.add_argument("--noise_level", nargs='?',default=-20, type=int, help="Nominal noise level (dBFS)")
    parser.add_argument("--ic_delay", nargs='?',default=80, type=int, help="IC x channel delay")
    args = parser.parse_args()
    return args


def main():
    start_time = time.time()
    args = parse_arguments()
    get_attenuation_c_py("test", args.noise_band, args.noise_level, args.angle, args.rt60)
    print("--- {0:.2f} seconds ---".format(time.time() - start_time))


if __name__ == "__main__":
    main()
