# Copyright 2021-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import numpy as np
import tempfile
import shutil
import configparser
from pathlib import Path
from test_wav import test_wav
import soundfile as sf

parser = configparser.ConfigParser()
parser.read(Path(__file__).parent / "parameters.cfg")
aec_xe = Path(__file__).parent / "bin" / "test_aec_enhancements.xe"
in_dir = parser.get("Folders", "in_dir")
(Path(__file__).parent / in_dir).mkdir(exist_ok=True)
out_dir = parser.get("Folders", "out_dir")
(Path(__file__).parent / out_dir).mkdir(exist_ok=True)

adapt_mode_dict = {'AEC_ADAPTION_AUTO':0, 'AEC_ADAPTION_FORCE_ON':1, 'AEC_ADAPTION_FORCE_OFF': 2}

dut_H_hat_file = "h_hat.bin"
runtime_args_file = "args.bin"
AEC_MAX_Y_CHANNELS = int(parser.get("Config", "y_channel_count"))
AEC_MAX_X_CHANNELS = int(parser.get("Config", "x_channel_count"))

def run_aec_xc(y_data, x_data, testname, adapt=-1, h_hat_dump=None, adapt_mode=adapt_mode_dict['AEC_ADAPTION_AUTO'], num_y_channels=AEC_MAX_Y_CHANNELS, num_x_channels=AEC_MAX_X_CHANNELS, target="xs3a"):
    input_file = Path(__file__).parent / in_dir / f"input_{testname}.wav"
    output_file = Path(__file__).parent / out_dir / f"output_{testname}.wav"
    #input wav file always has (AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS) channels, as per the build time aec configuration. Changing AEC config at runtime shouldn't affect input packing
    if(y_data.ndim == 1):
        y_data = np.atleast_2d(y_data).T
    if(x_data.ndim == 1):
        x_data = np.atleast_2d(x_data).T

    y_chans = y_data.shape[-1]
    x_chans = x_data.shape[-1]

    #All input wav files need to have AEC_MAX_Y_CHANNELS y channels and AEC_MAX_X_CHANNELS x channels since this is the configuration AEC is built with
    extra_y_chans = AEC_MAX_Y_CHANNELS - y_chans
    extra_x_chans = AEC_MAX_X_CHANNELS - x_chans
    #duplicate last column to get required no. of channels
    if extra_y_chans:
        extra_y = np.tile(y_data[:,[-1]], extra_y_chans)
        y_data = np.hstack((y_data, extra_y))
    if extra_x_chans:
        extra_x = np.tile(x_data[:,[-1]], extra_x_chans)
        x_data = np.hstack((x_data, extra_x))
    input_data = np.hstack((y_data, x_data))
    sf.write(input_file, input_data, 16000, format="WAV", subtype="PCM_32")

    with tempfile.TemporaryDirectory(dir=".") as tmp_folder:
        tmp_path = Path(tmp_folder)
        #write runtime arguments into args.bin
        with open(tmp_path / runtime_args_file, "wb") as fargs:
            fargs.write(f"y_channels {num_y_channels}\n".encode('utf-8'))
            fargs.write(f"x_channels {num_x_channels}\n".encode('utf-8'))
            fargs.write(f"stop_adapting {adapt}\n".encode('utf-8'))
            fargs.write(f"adaption_mode {adapt_mode}\n".encode('utf-8'))

        test_wav(aec_xe, input_file, output_file, 240, AEC_MAX_Y_CHANNELS, 240, target=target, tmp_folder=tmp_folder)

        if h_hat_dump is not None:
            shutil.copy2(tmp_path / dut_H_hat_file, h_hat_dump)

    return input_file, output_file


def get_h_hat(filename, aec):
    """Gets H_hat from XC H_hat dump

    WARNING: This could be dangerous, the filename argument is parsed as
    python when aec = 'xc'.
    """
    H_hat = None

    if aec == 'xc':
        shutil.copy2(filename, "temp.py")
        from temp import H_hat
    else:
        with open(filename, "rb") as f:
            H_hat = np.load(f)
    assert H_hat is not None
    return H_hat


