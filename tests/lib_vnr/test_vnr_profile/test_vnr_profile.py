# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

###### help
#### Running with lib xscope_filio implemented xscope host
# python host_app.py test_stream_1.wav vnr_out.bin

### To see the plot
# python host_app.py test_stream_1.wav vnr_out.bin --show-plot

import numpy as np
import matplotlib.pyplot as plt
import argparse
from pathlib import Path
from run_dut import run_dut
import soundfile as sf
from profile_xcore import parse_profile_log

cwd = Path(__file__).parent
exe = Path(__file__).parent / "bin" / "test_vnr_profile"


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_wav", nargs='?', help="input wav file")
    parser.add_argument("--show-plot", action='store_true', help="show the VNR output plot")
    parser.add_argument("--target", choices=["xs3a", "native"], default="xs3a", help="Run target")
    args = parser.parse_args()
    return args

def plot_result(vnr_out, out_file, show_plot=False):
    #Plot VNR output
    mant = np.array(vnr_out[0::2], dtype=np.float64)
    exp = np.array(vnr_out[1::2], dtype=np.int32)
    vnr_out = mant * (float(2)**exp)
    plt.plot(vnr_out)
    plt.xlabel('frames')
    plt.ylabel('vnr estimate')
    fig_instance = plt.gcf()
    if show_plot:
        plt.show()
    plotfile = f"vnr_example_plot_{Path(out_file).stem}.png"
    fig_instance.savefig(plotfile)

def run_with_xscope_fileio(input_file, target, parse_profile=False):

    input_data, _ = sf.read(input_file, dtype="int32")

    assert len(input_data.shape) == 1, "Input data can be single channel only"

    output_data, stdout = run_dut(input_data, exe, target)

    if target != "native" and parse_profile:
        src_folder = str(cwd / 'src')
        parse_profile_log(
            stdout,
            src_folder,
            worst_case_file="vnr_prof.log",
            per_tag_worst_case=True,
            recursive=False
        )

    return output_data

def test_vnr_profile():
    wav_name = "test_stream_1.wav"
    vnr_out = run_with_xscope_fileio(wav_name, "xs3a", True)
    plot_result(vnr_out, wav_name, False)

if __name__ == "__main__":
    args = parse_arguments()
    print(f"input_file: {args.input_wav}, show_plot={args.show_plot}, target={args.target}")
    vnr_out = run_with_xscope_fileio(args.input_wav, args.target, parse_profile=True)

    plot_result(vnr_out, args.input_wav, show_plot=args.show_plot)

