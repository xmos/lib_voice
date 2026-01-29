# Copyright 2021-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import configparser
import pytest
from aec_test_utils import get_test_instances, read_config

import os
import tempfile
import numpy as np
import shutil
from pathlib import Path
import soundfile as sf
from run_dut import run_with_xscope_fileio

parser = configparser.ConfigParser()
parser.read("parameters.cfg")

in_dir   = parser.get("Folders", "in_dir")
out_dir = parser.get("Folders", "out_dir")
(Path(__file__).parent / out_dir).mkdir(exist_ok=True)

y_channel_count = parser.get("Config", "y_channel_count")
x_channel_count = parser.get("Config", "x_channel_count")
phases = parser.get("Config", "phases")

aec_xe = Path(__file__).parent / "bin" / "test_aec_spec.xe"

dut_in_wav = "input.wav"
dut_out_wav = "output.wav"
runtime_args_file = "args.bin"
dut_H_hat_file = "H_hat.bin"
def run_aec_xc(audio_in, audio_ref, audio_out, adapt=-1, h_hat_dump=None):
    y_data, rate = sf.read(audio_in, dtype="int32", always_2d=True)
    x_data, rate = sf.read(audio_ref, dtype="int32", always_2d=True)
    data = np.hstack((y_data, x_data)) #mic+ref

    sf.write(dut_in_wav, data, rate)

    with tempfile.TemporaryDirectory(dir=".") as tmp_folder:

        sf.write(os.path.join(tmp_folder, dut_in_wav), data, rate, "PCM_32")
    
        with open(os.path.join(tmp_folder, runtime_args_file), "wb") as ref_file:
            ref_file.write(f"stop_adapting {adapt}".encode('utf-8'))

        stdo = run_with_xscope_fileio(aec_xe, tmp_folder)

        #test_check_output expects a 2 channel output despite building AEC for 1 y channel, so convert dut output to 2ch
        data, rate = sf.read(os.path.join(tmp_folder, dut_out_wav), dtype="int32", always_2d=True)
        data = np.hstack((data, data))

        if h_hat_dump != None:
            shutil.copy2(os.path.join(tmp_folder, dut_H_hat_file), h_hat_dump)

    sf.write(audio_out, data, rate, "PCM_32")


@pytest.fixture
def test_type(request):
    test_name = request.node.name
    test_type = test_name[len("test_process_"):test_name.index('[')]
    return test_type


@pytest.fixture
def test_config(test_type):
    return read_config(test_type)


@pytest.mark.parametrize('test', get_test_instances('simple', in_dir, out_dir))
def test_process_simple(test):
    run_aec_xc(test['in_filename'], test['ref_filename'],
               test['out_filename'])


@pytest.mark.parametrize('test', get_test_instances('multitone', in_dir,
                                                    out_dir))
def test_process_multitone(test):
    run_aec_xc(test['in_filename'], test['ref_filename'],
               test['out_filename'])


@pytest.mark.parametrize('test', get_test_instances('excessive', in_dir,
                                                    out_dir))
def test_process_excessive(test):
    run_aec_xc(test['in_filename'], test['ref_filename'],
               test['out_filename'])


@pytest.mark.parametrize('test', get_test_instances('impulseresponse', in_dir,
                                                    out_dir))
def test_process_impulseresponse(test, test_config):
    stop_adapt_frame = (test_config['settle_time'] * 16000) // 240
    h_hat_xc = os.path.join(out_dir, test['id'] + "-h_hat.py")

    run_aec_xc(test['in_filename'], test['ref_filename'],
               test['out_filename'], stop_adapt_frame,
               h_hat_xc)


@pytest.mark.parametrize('test', get_test_instances('smallimpulseresponse',
                                                    in_dir, out_dir))
def test_process_smallimpulseresponse(test, test_config):
    stop_adapt_frame = (test_config['settle_time'] * 16000) // 240
    h_hat_xc = os.path.join(out_dir, test['id'] + "-h_hat.py")

    run_aec_xc(test['in_filename'], test['ref_filename'],
               test['out_filename'], stop_adapt_frame,
               h_hat_xc)


@pytest.mark.parametrize('test', get_test_instances('bandlimited', in_dir,
                                                    out_dir))
def test_process_bandlimited(test, test_config):
    stop_adapt_frame = (test_config['settle_time'] * 16000) // 240
    run_aec_xc(test['in_filename'], test['ref_filename'],
               test['out_filename'], stop_adapt_frame)
