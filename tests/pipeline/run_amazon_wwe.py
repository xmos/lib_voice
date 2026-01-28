# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import os
import subprocess
import argparse
import sys
import tempfile
import shutil
import stat
from pathlib import Path


if sys.platform == "darwin":
    assert(False), "amazon_wwe filesim executable runs only on x86 Linux"
else:
    WW_FILESIM_EXE = "amazon_ww_filesim"

WW_MODEL = "WR_250k.en-US.alexa.bin"

def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=str, nargs='?', help="input wav file")
    args = parser.parse_args()
    return args

def run_file(input_filename, model):
    try:
        wwe_path = Path(os.environ['AMAZON_WWE_PATH']).expanduser()
        print("wwe_path = %s"%(wwe_path))
    except:
        wwe_path = Path(__file__).parents[3] / "amazon_wwe"
        print('env variable AMAZON_WWE_PATH not set. looking for Amazon WWE in the default path ', wwe_path)

    filesim_exe = wwe_path / "x86" / WW_FILESIM_EXE
    ww_model = wwe_path / "models" / "common" / model
    if not filesim_exe.is_file():
        print('filesim executable not present in %s ', filesim_exe)
        assert(False)
    if not ww_model.is_file():
        print('model not present in %s ',ww_model)
        assert(False)

    #There is an issue when lots of instances running the same kw bin, so make a copy and run own version
    with tempfile.TemporaryDirectory(dir=".") as tmp_folder:
        tmp_folder = Path(tmp_folder)

        shutil.copyfile(filesim_exe, tmp_folder / "kw_bin")
        os.chmod(tmp_folder / "kw_bin", stat.S_IXUSR)
        shutil.copyfile(ww_model, tmp_folder / "kw_model")
        # There's this really srtange error where if the input stream path starts with a /, amazon_ww_filesim issues a warning, Warning: Can't open file and detects 0 keywords
        shutil.copy2(input_filename, tmp_folder)
        os.system(f"echo {input_filename.name} > {tmp_folder}/list.txt")

        run_cmd = '%s list.txt -t 500 -m %s' %("./kw_bin", "kw_model")
        print("run_cmd = ", run_cmd)
        output = subprocess.check_output(run_cmd, shell=True, cwd=tmp_folder)

    # Compute the number of occurrences of 'alexa' to get the number of detection
    detections = len(output.decode().split(f"{input_filename.stem}: 'ALEXA'")) - 1
    return detections

def run_amazon_wwe(input_filename):
    detections = run_file(input_filename, WW_MODEL)
    return detections

if __name__ == "__main__":
    args = parse_arguments()
    assert(args.input != None), "Specify Input wav file"
    detections = run_amazon_wwe(Path(args.input))
    print("detections = %d"%(detections))
