# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import os
import numpy as np
import subprocess
from pathlib import Path
from shutil import copyfile, rmtree

from .prepare_aec_input_file import prepare_input_file
from .delay_analyser import delay_analyser
from .delay_analyser import FRAME_ADVANCE
import tempfile

from run_dut import run_with_xscope_fileio

source_wav_file_rate = 48000
voice_sample_rate = 16000
delay_output_file_name = "requested_delay_samples.bin"

test_exe = Path(__file__).parent / "bin" / "test_adec.xe"


def generate_random_delay_changes(number, spacing, min_s, max_s):
  delays = []
  for change in range(number):
    this_delay = np.random.uniform(min_s, max_s, 1)[0] * source_wav_file_rate
    this_time_samps = spacing * (1 + change) * source_wav_file_rate
    delays.append((int(this_time_samps), int(this_delay)))
  return delays

def run_test(pipeline_config, info, path_to_regression_files, input_audio_files, far_end_delay_changes, test_length_s=70, run_target="xcore", volume_changes=None):
  assert run_target == "xcore", "Only xcore testing is supported"

  # tmp_dir = tempfile.mkdtemp(prefix='tmp_', dir='.')
  with tempfile.TemporaryDirectory(dir=".", prefix="tmp_") as tmp_dir:

    #write runtime arguments into args.bin. TODO send as config from caller
    with open(os.path.join(tmp_dir, "args.bin"), "wb") as fargs:
        fargs.write(f"y_channels {pipeline_config['num_y_channels']}\n".encode('utf-8'))
        fargs.write(f"x_channels {pipeline_config['num_x_channels']}\n".encode('utf-8'))
        fargs.write(f"main_filter_phases {pipeline_config['num_main_filter_phases']}\n".encode('utf-8'))
        fargs.write(f"shadow_filter_phases {pipeline_config['num_shadow_filter_phases']}\n".encode('utf-8'))

    aec_input_file = os.path.join(tmp_dir, "stage_a_input_16k.wav")
    ground_truth_file = os.path.join(tmp_dir, "ground_truth.txt")
    if far_end_delay_changes is not None:
      gt_changes = len(far_end_delay_changes)
      input_audio_dir = os.path.join(path_to_regression_files, 'input_audio_to_room_model')
      model_dir = os.path.join(path_to_regression_files, 'room_model')
      output_audio_dir = tmp_dir

      prepare_input_file(input_audio_files, input_audio_dir, model_dir, output_audio_dir, far_end_delay_changes, max_seconds=test_length_s, volume_changes=volume_changes)
      test_name = info + ", " + input_audio_files[0] + ", " + input_audio_files[1]
    else:
      copyfile(input_audio_files, aec_input_file)
      ground_truth_file_delays = input_audio_files.strip(".wav") + (".delays")
      copyfile(ground_truth_file_delays, ground_truth_file)

      gt_changes = 0
      test_name = info + ", " + input_audio_files.split('/')[-1]

    print ("run_target = ", run_target, ", tmp_dir = ", tmp_dir)
    copyfile(aec_input_file, os.path.join(tmp_dir, "input.wav")) #Axe sim has fixed file name input

    run_with_xscope_fileio(test_exe, tmp_dir)

    # Read estimated delay samples for every frame
    with open(os.path.join(tmp_dir, delay_output_file_name), 'r') as f:
      estimated_delay_samples = np.array([int(l) for l in f.readlines()], dtype=float)
    estimates = estimated_delay_samples / float(voice_sample_rate)


    #Scale estimates file to seconds
    xc_sim_de_file = os.path.join(tmp_dir, "xc_sim_delays_s.txt")
    print("estimates = ",estimates)
    estimates.tofile(xc_sim_de_file, sep="\n")

    xcore_delay_analyser = delay_analyser(FRAME_ADVANCE, ground_truth_file, xc_sim_de_file)
    report_summary = xcore_delay_analyser.analyse_events()
    report_summary['test_name'] = "Xcore: " + test_name
    report_summary['gt_changes'] = gt_changes

  graph_file_name = test_name + "_xcore.png"
  print(graph_file_name)
  xcore_delay_analyser.graph_delays(file_name=graph_file_name)

  # rmtree(tmp_dir)

  return report_summary


