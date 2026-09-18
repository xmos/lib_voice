# Copyright 2021-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
"""Runs the AEC spec: generate (see conftest.py) -> run DUT -> check output -> plot/log, all in
one pytest module so `pytest` (or a whole-directory sweep like `pytest lib_aec`) runs and reports
the whole pipeline directly, with no separate generate/parse/evaluate scripts or intermediate
junit files needed.
"""
import configparser
import importlib.util
import os
import shutil
import tempfile
from pathlib import Path

import numpy as np
import pytest
import soundfile as sf

from aec_test_utils import (get_test_instances, read_config, read_wav,
                            check_aec_output, get_criteria,
                            get_h_hat_impulse_response)
from plot_test import plot_test, plot_impulseresponse_test
from test_wav import test_wav

# Resolve every path relative to this directory (not the process cwd), so this suite also works
# when pytest is invoked from a parent directory, e.g. `pytest lib_aec` from tests/.
HERE = Path(__file__).parent

parser = configparser.ConfigParser()
parser.read(HERE / "parameters.cfg")

in_dir = str(HERE / parser.get("Folders", "in_dir"))
out_dir = str(HERE / parser.get("Folders", "out_dir"))
results_dir = str(HERE / parser.get("Folders", "results_dir"))
Path(out_dir).mkdir(exist_ok=True)

plot_dir_pass = os.path.join(results_dir, "plots")
plot_dir_fail = os.path.join(plot_dir_pass, "fail")
log_dir_pass = os.path.join(results_dir, "logs")
log_dir_fail = os.path.join(log_dir_pass, "fail")
for _dir in (plot_dir_pass, plot_dir_fail, log_dir_pass, log_dir_fail):
    Path(_dir).mkdir(parents=True, exist_ok=True)

aec_xe = HERE / "bin" / "test_aec_spec.xe"

dut_in_wav = "input.wav"
dut_out_wav = "output.wav"
runtime_args_file = "args.bin"
dut_H_hat_file = "h_hat.bin"


def run_aec_xc(audio_in, audio_ref, audio_out, adapt=-1, h_hat_dump=None, target="xs3a"):
    y_data, rate = sf.read(audio_in, dtype="int32", always_2d=True)
    x_data, rate = sf.read(audio_ref, dtype="int32", always_2d=True)
    data = np.hstack((y_data, x_data)) #mic+ref
    frame_advance = 240

    with tempfile.TemporaryDirectory(dir=out_dir) as tmp_folder:
        tmp_path = Path(tmp_folder)
        input_file = tmp_path / dut_in_wav
        output_file = tmp_path / dut_out_wav
        AEC_MAX_Y_CHANNELS = 1

        sf.write(input_file, data, rate, "PCM_32")

        with open(tmp_path / runtime_args_file, "wb") as ref_file:
            ref_file.write(f"stop_adapting {adapt}".encode('utf-8'))

        stdout = test_wav(aec_xe, input_file, output_file, frame_advance, AEC_MAX_Y_CHANNELS,
                          frame_advance, target=target, tmp_folder=tmp_folder)

        #test_check_output expects a 2 channel output despite building AEC for 1 y channel, so convert dut output to 2ch
        data, rate = sf.read(output_file, always_2d=True)
        data = np.hstack((data, data))

        if h_hat_dump is not None:
            shutil.copy2(tmp_path / dut_H_hat_file, h_hat_dump)

    sf.write(audio_out, data, rate, "PCM_32")
    return stdout


def get_h_hat(filename, unique_id):
    """Loads H_hat from an XC H_hat dump (a python source snippet, see dump_H_hat.c).

    Uses a per-test temp module name/file so concurrent xdist workers don't clash.
    """
    tmp_path = Path(out_dir) / f"_h_hat_{unique_id}.py"
    shutil.copy2(filename, tmp_path)
    spec = importlib.util.spec_from_file_location(f"_h_hat_{unique_id}", tmp_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    tmp_path.unlink()
    assert module.H_hat is not None
    return module.H_hat


def write_log(passed, test_id, stdout_lines):
    log_dir = log_dir_pass if passed else log_dir_fail
    with open(os.path.join(log_dir, f"{test_id}.log"), 'w') as f:
        f.write("".join(stdout_lines))


@pytest.mark.parametrize('test', get_test_instances('simple', in_dir, out_dir))
def test_simple(test, target):
    cfg = read_config(test['test_type'])
    stdout = run_aec_xc(test['in_filename'], test['ref_filename'],
                        test['out_filename'], target=target)

    audio_in = read_wav(test['in_filename'])
    audio_ref = read_wav(test['ref_filename'])
    audio_out = read_wav(test['out_filename'])[:, 0]
    criteria = get_criteria(test['id'])
    passed = check_aec_output(audio_in, audio_ref, audio_out,
                              cfg['start_fft'], cfg['end_fft'], criteria,
                              frequencies=cfg['frequencies'])

    plot_test(os.path.join(plot_dir_pass if passed else plot_dir_fail, f"{test['id']}.png"),
              test['id'], test['in_filename'], test['ref_filename'], test['out_filename'],
              cfg['settle_time'])
    write_log(passed, test['id'], stdout)
    assert passed


@pytest.mark.parametrize('test', get_test_instances('multitone', in_dir, out_dir))
def test_multitone(test, target):
    cfg = read_config(test['test_type'])
    stdout = run_aec_xc(test['in_filename'], test['ref_filename'],
                        test['out_filename'], target=target)

    audio_in = read_wav(test['in_filename'])
    audio_ref = read_wav(test['ref_filename'])
    audio_out = read_wav(test['out_filename'])[:, 0]
    criteria = get_criteria(test['id'])
    passed = check_aec_output(audio_in, audio_ref, audio_out,
                              cfg['start_fft'], cfg['end_fft'], criteria,
                              frequencies=cfg['frequencies'])

    plot_test(os.path.join(plot_dir_pass if passed else plot_dir_fail, f"{test['id']}.png"),
              test['id'], test['in_filename'], test['ref_filename'], test['out_filename'],
              cfg['settle_time'])
    write_log(passed, test['id'], stdout)
    assert passed


@pytest.mark.parametrize('test', get_test_instances('excessive', in_dir, out_dir))
def test_excessive(test, target):
    cfg = read_config(test['test_type'])
    stdout = run_aec_xc(test['in_filename'], test['ref_filename'],
                        test['out_filename'], target=target)

    audio_in = read_wav(test['in_filename'])
    audio_ref = read_wav(test['ref_filename'])
    audio_out = read_wav(test['out_filename'])[:, 0]
    criteria = get_criteria(test['id'])
    # Excessive tests exercise the failure path of check_aec_output - a suppressed check IS the pass.
    passed = not check_aec_output(audio_in, audio_ref, audio_out,
                                  cfg['start_fft'], cfg['end_fft'], criteria,
                                  frequencies=cfg['frequencies'])

    plot_test(os.path.join(plot_dir_pass if passed else plot_dir_fail, f"{test['id']}.png"),
              test['id'], test['in_filename'], test['ref_filename'], test['out_filename'],
              cfg['settle_time'])
    write_log(passed, test['id'], stdout)
    assert passed


@pytest.mark.parametrize('test', get_test_instances('impulseresponse', in_dir, out_dir))
def test_impulseresponse(test, target):
    cfg = read_config(test['test_type'])
    stop_adapt_frame = (cfg['settle_time'] * 16000) // 240
    h_hat_dump = os.path.join(out_dir, test['id'] + "-h_hat.py")

    stdout = run_aec_xc(test['in_filename'], test['ref_filename'], test['out_filename'],
                        stop_adapt_frame, h_hat_dump, target=target)

    h_hat = get_h_hat(h_hat_dump, test['id'].replace(',', '_'))
    h_hat_ir = get_h_hat_impulse_response(h_hat, 0, 0)
    plot_impulseresponse_test(os.path.join(plot_dir_pass, f"{test['id']}.png"),
                              test['id'], test['echo'], h_hat_ir, test['headroom'],
                              test['out_filename'], cfg['settle_time'])
    write_log(True, test['id'], stdout)
    # TODO: no numeric pass/fail criteria defined yet for impulse response tests, only plotted.


@pytest.mark.parametrize('test', get_test_instances('smallimpulseresponse', in_dir, out_dir))
def test_smallimpulseresponse(test, target):
    cfg = read_config(test['test_type'])
    stop_adapt_frame = (cfg['settle_time'] * 16000) // 240
    h_hat_dump = os.path.join(out_dir, test['id'] + "-h_hat.py")

    stdout = run_aec_xc(test['in_filename'], test['ref_filename'], test['out_filename'],
                        stop_adapt_frame, h_hat_dump, target=target)

    h_hat = get_h_hat(h_hat_dump, test['id'].replace(',', '_'))
    h_hat_ir = get_h_hat_impulse_response(h_hat, 0, 0)
    plot_impulseresponse_test(os.path.join(plot_dir_pass, f"{test['id']}.png"),
                              test['id'], test['echo'], h_hat_ir, test['headroom'],
                              test['out_filename'], cfg['settle_time'])
    write_log(True, test['id'], stdout)
    # TODO: no numeric pass/fail criteria defined yet for impulse response tests, only plotted.


@pytest.mark.parametrize('test', get_test_instances('bandlimited', in_dir, out_dir))
def test_bandlimited(test, target):
    cfg = read_config(test['test_type'])
    stop_adapt_frame = (cfg['settle_time'] * 16000) // 240
    stdout = run_aec_xc(test['in_filename'], test['ref_filename'],
                        test['out_filename'], stop_adapt_frame, target=target)

    audio_in = read_wav(test['in_filename'])
    audio_ref = read_wav(test['ref_filename'])
    audio_out = read_wav(test['out_filename'])[:, 0]
    criteria = get_criteria(test['id'])
    passed = check_aec_output(audio_in, audio_ref, audio_out,
                              cfg['start_fft'], cfg['end_fft'], criteria,
                              frequencies=cfg['frequencies'])

    plot_test(os.path.join(plot_dir_pass if passed else plot_dir_fail, f"{test['id']}.png"),
              test['id'], test['in_filename'], test['ref_filename'], test['out_filename'],
              cfg['settle_time'])
    write_log(passed, test['id'], stdout)
    assert passed
