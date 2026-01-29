# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import os
from pathlib import Path
import tempfile

import matplotlib.pyplot as plt
import numpy as np
import pytest
import soundfile as sf

from py_voice.scripts.room_acoustic_pipeline.room_acoustic_pipeline import helpers
import py_voice.scripts.room_acoustic_pipeline.room_acoustic_pipeline.core as rap

import py_voice.test.ic.ic_test_helpers as ith
from py_voice.config import config
from py_voice.core import leq_smooth

import py_vs_c_utils as pvc
from run_dut import run_with_xscope_fileio

# some mess to get the list of IRs
hydra_audio_path = Path(os.environ.get('hydra_audio_PATH', '~/hydra_audio')).expanduser()

imp_path = hydra_audio_path / 'acoustic_team_test_audio' / 'impulse'
imp_list = helpers.files_of_type(imp_path, 'npy')

# some possible parameters
gain = 24
noise_level = gain
noise_pos = 2
speech_name = "007_podcast"
speech_pos = 3

xe = Path(__file__).parent / "bin" / "test_ic_bad_state.xe"
ap_config_file = Path(__file__).parents[2] / "shared" / "config" / "ic_conf_no_adapt_control.json"
ap_conf = config.get_config_dict(ap_config_file)
cwd = Path(__file__).parent

def run_target(input_data, conf_data):
    output_data = np.empty(0, dtype=np.int32)

    with tempfile.TemporaryDirectory(dir=".") as tmp_folder:
        tmp_folder = Path(tmp_folder)

        input_file = tmp_folder / "input.bin"
        input_data.astype(np.int32).tofile(input_file)

        conf_file = tmp_folder / "conf.bin"
        conf_data.astype(np.int32).tofile(conf_file)

        run_with_xscope_fileio(xe, tmp_folder)

        output_file = tmp_folder / "output.bin"
        output_data = np.fromfile(output_file, dtype=np.int32)

    return output_data

def run_test(input_data, conf_data, test_name, fs):

    output_data = run_target(input_data, conf_data)
    output_data = pvc.int32_to_float(output_data)

    sf.write(cwd / f"output_{test_name}.wav", output_data, fs)

    # # check after 3 seconds we have converged to be better than the fixed good filter (because it should leak)
    t, leq = leq_smooth(output_data, fs, 0.05)
    average = np.mean(leq[(t>3)*(t<5)])
    return average

def form_conf_data(config, H_hat, num_words_H):
    conf_data = np.empty(0, dtype=np.int32)
    conf_data = np.append(conf_data, np.array([num_words_H, config], dtype=np.int32))
    conf_data = np.append(conf_data, np.array(pvc.float_to_int32_qxx(pvc.flatten_complex_array(H_hat), 29), dtype=np.int32))
    return conf_data

@pytest.mark.parametrize("room", ["lab"])
@pytest.mark.parametrize("speech_level", [0])
@pytest.mark.parametrize("noise_name", ["006_Pink", "015_Silence"])
def test_bad_state(room, speech_level, noise_name):

    # some constants:
    length_secs = 10

    # load config
    fs = ap_conf["general"]["fs"]
    proc_frame_length = ap_conf["general"]["proc_frame_length"]
    frame_advance = ap_conf["general"]["frame_advance"]

    delay = ap_conf["ic"]["y_channel_delay"]
    phases = ap_conf["ic"]["phases"]
    f_bin_count = (proc_frame_length // 2) + 1

    # make room pipeline spec
    noise_spec, speech_spec, playback_spec, length_samps = rap.make_rap_spec(fs, room,
                                                                noise_name=noise_name,
                                                                noise_level=noise_level,
                                                                noise_pos=noise_pos,
                                                                speech_name=speech_name,
                                                                speech_pos=speech_pos,
                                                                speech_level=speech_level,
                                                                length_secs=length_secs)
    noise_ir = np.load(imp_path / imp_list[noise_spec[2]])
    speech_ir = np.load(imp_path / imp_list[speech_spec[2]])

    ideal_speech_cancellation_H = ith.calc_ideal_fd_filter(speech_ir, delay, phases, f_bin_count, proc_frame_length, frame_advance)[0, 0, :, :]
    ideal_noise_cancellation_H = ith.calc_ideal_fd_filter(noise_ir, delay, phases, f_bin_count, proc_frame_length, frame_advance)[0, 0, :, :]

    # run room pipeline
    mic_sig, out_array, in_array = rap.room_sim(utterance=speech_spec,
                                                point_noise=noise_spec,
                                                signal_len=length_samps,
                                                return_unsquashed=True,
                                                return_signals=True)

    # The xcore pipeline is fixed-point; fail fast if the float sim would clip when
    # converted to int32 (keep good/bad at the same scale; do not normalise here).
    peak = float(np.max(np.abs(mic_sig)))
    target_peak = 0.95
    assert peak <= target_peak, (
        f"Input signal too hot for fixed-point: peak={peak:.6f} > {target_peak}. "
        "Reduce `gain`/levels or adjust the RAP spec to avoid clipping."
    )
    num_words_H = f_bin_count * 2 * phases # H_hat[ph][bin_count] for both real and complex

    # crop to have full frames
    inx = mic_sig.shape[1] // frame_advance * frame_advance
    mic_sig = mic_sig[:, :inx]

    sf.write(cwd / f"input_{noise_name}.wav", mic_sig.T, fs)
    input_data = pvc.float_to_int32(mic_sig)
    input_data = pvc.interleave_channel_frames(input_data, frame_advance)

    conf_data_cancel_noise = form_conf_data(2, ideal_noise_cancellation_H, num_words_H)
    average_fixed_good = run_test(input_data, conf_data_cancel_noise, f"good_{noise_name}", fs)

    conf_data_cancel_speech = form_conf_data(0, ideal_speech_cancellation_H, num_words_H)
    average_adapt_bad = run_test(input_data, conf_data_cancel_speech, f"bad_{noise_name}", fs)

    print(f"average_adapt_bad (dB): {average_adapt_bad}")
    print(f"average_fixed_good (dB): {average_fixed_good}")
    print(f"Assertion: adapt_bad > fixed_good: {average_adapt_bad} > {average_fixed_good} = {average_adapt_bad > average_fixed_good}")
    assert average_adapt_bad > average_fixed_good


if __name__ =="__main__":
    test_bad_state("lab", 0, "006_Pink")
