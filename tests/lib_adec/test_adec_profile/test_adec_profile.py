# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import os
import tempfile
import shutil
import pytest
from pathlib import Path
from profile_xcore import parse_profile_log
from run_dut import run_with_xscope_fileio

src_folder = Path(__file__).parents[1] / "test_wav_adec" / "src"

hydra_audio_path = Path(os.environ.get('hydra_audio_PATH', '~/hydra_audio')).expanduser()
wav_input_file = hydra_audio_path / "adec_profile_test_stream" / "input_aec_delay_change_short.wav"

xc_out_file_name = "output.wav"
def run_pipeline_xe(pipeline_xe, run_config, threads):

    with tempfile.TemporaryDirectory(dir=".") as tmp_folder:
        tmp_folder = Path(tmp_folder)

        shutil.copy2(wav_input_file, tmp_folder / "input.wav")

        with open(tmp_folder / "args.bin", "wb") as fargs:
            fargs.write(f"main_filter_phases {run_config.num_main_filt_phases}\n".encode('utf-8'))
            fargs.write(f"shadow_filter_phases {run_config.num_shadow_filt_phases}\n".encode('utf-8'))
            fargs.write(f"y_channels {run_config.num_y_channels}\n".encode('utf-8'))
            fargs.write(f"x_channels {run_config.num_x_channels}\n".encode('utf-8'))

        xcore_stdo = run_with_xscope_fileio(pipeline_xe, tmp_folder)

        config_name = f"{threads}_{run_config.num_y_channels}_{run_config.num_x_channels}_{run_config.num_main_filt_phases}_{run_config.num_shadow_filt_phases}"
        shutil.copy2(tmp_folder / "output.wav", Path(__file__).parent / f"output_{config_name}.wav")

    config_info = f"Config: Threads ({threads}), Y_channels ({run_config.num_y_channels}), X_channels ({run_config.num_x_channels}), Main filter phases ({run_config.num_main_filt_phases}), Shadow filter phases ({run_config.num_shadow_filt_phases})"

    parse_profile_log(
        xcore_stdo,
        src_folder,
        worst_case_file=f"adec_prof_{run_config.config_str()}_{threads}threads.log",
        config_info=config_info
    )

class aec_config:
    def __init__(self, config_str):
        config = config_str.split()
        assert len(config) == 4, "Incorrect length config specified!"
        self.num_y_channels = config[0]
        self.num_x_channels = config[1]
        self.num_main_filt_phases = config[2]
        self.num_shadow_filt_phases = config[3]
    def print_config(self):
        print("Config = ", self.num_y_channels,  self.num_x_channels, self.num_main_filt_phases, self.num_shadow_filt_phases)
    def config_str(self):
        return f"{self.num_y_channels}ych_{self.num_x_channels}xch_{self.num_main_filt_phases}mainph_{self.num_shadow_filt_phases}shadph"


xe_files = (Path(__file__).parent / "bin").rglob('*.xe')

@pytest.fixture(scope="session", params=xe_files)
def setup(request):
    xe = request.param
    # extract stem part of filename
    # This should give a string of the form test_aec_profile_<threads>_<ychannels>_<xchannels>_<mainphases>_<shadowphases>
    name  = xe.stem
    config = (f"{name}".split('_'))[-5:] #Split by _ and pick up the last 5 values to get the config
    threads = config[0]
    rest_of_config = ' '.join(config[1:]) #remaining build config in "<ych> <xch> <mainph> <shadowph>" form
    return xe, aec_config(rest_of_config), threads

#For every build_config, test with all specified run time configs
@pytest.mark.parametrize("run_config", ['', '1 2 15 5'])
def test_profile(setup, run_config):
    #run_config is the aec runtime configuration specified in '<num_y_channels> <num_x_channels> <num_main_filter_phases> <num_shadow_filter_phases>' format
    #if run_config is an empty string, run the configuration that was built
    print(f"config {run_config}")
    pipeline_xe, build_config, threads = setup
    if run_config == '':
        #test the configuration that was built
        print(f'test build_config')
        run_pipeline_xe(pipeline_xe, build_config, threads) #threads is passed in only for logging purposes
    else:
        #test the specified run time configuration
        run_config = aec_config(run_config)
        run_pipeline_xe(pipeline_xe, run_config, threads)
        print('test run_config')
