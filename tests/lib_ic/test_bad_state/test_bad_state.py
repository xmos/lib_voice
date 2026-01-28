# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import os
import sys
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

sys.path.append('../../shared/python/')
import py_vs_c_utils as pvc
import xtagctl
import xscope_fileio
from run_dut import run_with_xscope_fileio

# some mess to get the list of IRs
home = Path(os.environ.get('hydra_audio_PATH', '~/hydra_audio'), "acoustic_team_test_audio")

audio_dirs = ['speech', 'point_noise', 'playback_audio', 'ambient_noise']
audio_path = [home / ad for ad in audio_dirs]
audio_list = [helpers.files_of_type(ap, 'wav') for ap in audio_path]

imp_path = home / 'impulse'
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

def run_xcore(conf_data, out_name, cwd='.'):
    conf_file = Path(cwd, 'conf.bin')
    output_file = Path(cwd, 'output.wav')

    conf_data.astype(np.int32).tofile(conf_file)

    run_with_xscope_fileio(xe, cwd)

    out_data_int32, sr = sf.read(output_file, dtype='int32')
    out_data = pvc.int32_to_float(out_data_int32)
    os.replace(output_file, out_name)
    os.remove(conf_file)
    return sr, out_data


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

    with tempfile.TemporaryDirectory(dir='.') as tmpdirname:

        sf.write(Path(tmpdirname, 'input.wav'), pvc.float_to_int32(mic_sig.T), fs, subtype='PCM_32')

        # initialise IC to cancel the speech, auto adaptation
        conf_data_cancel_speech = form_conf_data(0, ideal_speech_cancellation_H, num_words_H)
        sr, out_data_adapt_bad = run_xcore(conf_data_cancel_speech, 'output_bad_' + noise_name + '.wav', cwd=tmpdirname)


    with tempfile.TemporaryDirectory(dir='.') as tmpdirname:
        sf.write(Path(tmpdirname, 'input.wav'), pvc.float_to_int32(mic_sig.T), fs, subtype='PCM_32')

        # initialise IC to cancel the noise, don't adapt
        conf_data_cancel_noise = form_conf_data(2, ideal_noise_cancellation_H, num_words_H)
        sr, out_data_fixed_good = run_xcore(conf_data_cancel_noise, 'output_good_' + noise_name + '.wav', cwd=tmpdirname)

        os.replace(Path(tmpdirname, 'input.wav'), Path('input_' + noise_name + '.wav'))

    t, adapt_bad = leq_smooth(out_data_adapt_bad, fs, 0.05)
    t, fixed_good = leq_smooth(out_data_fixed_good, fs, 0.05)

    # check after 3 seconds we have converged to be better than the fixed good filter (because it should leak)
    average_fixed_good = np.mean(fixed_good[(t>3)*(t<5)])
    average_adapt_bad  = np.mean(adapt_bad[(t>3)*(t<5)])
    print(f"average_adapt_bad (dB): {average_adapt_bad}")
    print(f"average_fixed_good (dB): {average_fixed_good}")
    print(f"Assertion: adapt_bad > fixed_good: {average_adapt_bad} > {average_fixed_good} = {average_adapt_bad > average_fixed_good}")
    assert average_adapt_bad > average_fixed_good

    if __name__ == "__main__":
        t2, original_speech = leq_smooth(out_array[0, delay:, 0], fs, 0.05)
        plt.plot(t, adapt_bad, label="adapt_bad")
        plt.plot(t, fixed_good, label="fixed_good")
        plt.plot(t2, original_speech, linestyle='--', label="raw_speech")
        plt.ylim(np.array([-10, 40])-50)
        plt.ylabel("level (dB)")
        plt.xlabel("Time (s)")
        plt.xlim([0, 5])
        plt.title("Input-output VNR, lab podcast, pink noise, speech level %d dB"%speech_level)
        plt.legend()
        plt.grid()
        plt.show()


if __name__ =="__main__":
    test_bad_state("lab", 0, "006_Pink")
