# Copyright 2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
from pathlib import Path
from profile_xcore import parse_profile_log
import numpy as np
import re
import json
import pytest
import tempfile
from run_dut import run_dut
import py_vs_c_utils as pvc
import input_generators

"""
MIPS profiling test
===================

This module provides pytest-based profiling tests for voice processing
modules. Executables are discovered dynamically and profiled using
generated audio input signals.

Requirements
------------

Each module must:

- Have an executable named `app_mips_<module>.xe`. Read documentation in src/main.c to see
  how to add a new module in the test app.
- Have a corresponding entry in `MODULES`
- Provide a generator function in `input_generators`
  that accepts `frame_advance` and returns float audio
- All generators must return:
    - shape (samples,) for single-channel
    - shape (channels, samples) for multi-channel
    - float32 or float64 in range [-1.0, 1.0)
"""

"""
Registry of supported modules.
Each module configuration dictionary contains:

generator : str
    Name of the input generator function in the `input_generators`
    module. The function must accept a single argument
    `frame_advance` and return floating-point audio data in the range [-1.0, 1.0)
    in (samples, ) or (channels, samples) shape for single or multichannel audio data.

channels : int
    Expected number of input channels.
    - 1 for single-channel audio (shape: (samples,))
    - N for multi-channel audio (shape: (channels, samples))

frame_advance : int
    Frame advance in number of samples
"""
MODULES = {
    "ns": {
        "generator": "generate_ns_test_audio",
        "channels": 1,
        "frame_advance": 240
    },
    "agc": {
        "generator": "generate_agc_test_audio",
        "channels": 1,
        "frame_advance": 240
    },
    "ic": {
        "generator": "generate_ic_test_audio",
        "channels": 2,
        "frame_advance": 240
    },
    "vnr": {
        "generator": "generate_vnr_test_audio",
        "channels": 1,
        "frame_advance": 240
    },
    "aec": {
        "generator": "generate_aec_test_audio",
        "channels": 4,
        "frame_advance": 240
    },
    "adec": {
        "generator": "generate_adec_test_audio",
        "channels": 4,
        "frame_advance": 240
    },
}

def gen_input_and_run_dut(xe, module, target="xs3a"):
    """
    Generate input audio for a module and run the corresponding DUT xe.

    Parameters
    ----------
    xe : pathlib.Path
        Path to the executable under test.
    module : str
        Module name (must exist in MODULES).

    Returns
    -------
    str
        Standard output from DUT execution.

    Notes
    -----

    All applications targeting the same module
    (e.g. app_mips_aec_alt_arch_1thread and app_mips_aec_std_arch_1thread, both have the module as aec)
    are tested with the same input. The generator functions are per module and not per application.
    This might be a limitation if the requirement is to generate input per app. TBD: future improvement.
    """
    config = MODULES[module]

    generator_name = config["generator"]
    generator = getattr(input_generators, generator_name)
    input_data_float = generator(config["frame_advance"])
    input_data = pvc.float_to_int32(input_data_float)

    if config["channels"] == 1:
        assert input_data.ndim == 1, f"Module {module}, {config['generator']}() returned input data with incorrect number of dimensions"
    else:
        assert input_data.ndim == 2, f"Module {module}, {config['generator']}() returned input data with incorrect number of dimensions"
        assert input_data.shape[0] == config["channels"]
        input_data = pvc.interleave_channel_frames(
            input_data, config["frame_advance"]
        )

    _, xcore_stdo = run_dut(input_data, xe, target=target)
    return xcore_stdo


def find_apps():
    """
    Discover profiling executables matching registered modules.
    Only executables matching pattern `app_mips_<module>.xe`
    and present in MODULES are included.

    Returns
    -------
    list of tuple
        List of (xe_path, module_name) pairs.
    """
    target_xe = sorted((Path(__file__).parent / "bin").rglob("*.xe"))
    apps = []
    for xe in target_xe:
        app = xe.stem
        # Extract module name from app, for instance, extract 'aec' from 'app_mips_aec_std_arch_1thread'
        m = re.search(r'app_mips_([^\s^_]+)', app)
        if not m:
            continue
        module = m.group(1)
        if module in MODULES:
            apps.append((xe, module))
    return apps

# List all apps
APPS = find_apps()

@pytest.mark.parametrize(
    "xe,module",
    APPS,
    ids=[xe.stem for xe, _ in APPS]
)
def test_measure_mips(xe, module, pytestconfig, target):
    """
    Profile a single app (xe) and validate MIPS usage.

    Parameters
    ----------
    xe : pathlib.Path
        Executable under test.
    module : str
        Module name. Must be part of 'MODULES'
    pytestconfig : pytest.Config
        Pytest configuration object.

    Raises
    ------
    AssertionError
        If log format is invalid or app not in reference json.
    pytest.fail
        If measured MIPS for the app deviates beyond allowed threshold.

    Notes
    -----
    - Writes per-worker MIPS JSON output, for updating the reference JSON and RST in
      pytest_sessionfinish (if run with --update)
    - In non-update mode, validates against reference JSON. Threshold for deviation is 0.1 MIPS.
    """
    update = pytestconfig.getoption("--update")
    src_folder = Path(__file__).parent / "src"

    app = xe.stem # app name from executable
    print(f"app = {app}, module = {module}")
    log_file = Path(__file__).parent / f"{app}.log"
    xcore_stdo = gen_input_and_run_dut(xe, module, target)
    with tempfile.TemporaryDirectory(dir=".", suffix=app) as tmp_folder:
        tmp = Path(tmp_folder)
        parse_profile_log(
            xcore_stdo,
            src_folder,
            file_extensions=[f"*{module}*.c"],
            worst_case_file= log_file,
            profile_file=tmp / "parsed_profile.log",
            mapping_file=tmp / "profile_index_to_tag_mapping.log",
            exclude_init=True
        )
    text = Path(log_file).read_text()
    m = re.search(r'^MCPS\s+([0-9.]+)\s+MIPS', text, re.MULTILINE)
    assert m, (f"MIPS log file {log_file} doesnt seem to be formatted correctly. "
                f"file text = {text}")
    mips = float(m.group(1))
    # Dump {target: {app: mips}} in a json file, to be collected in pytest_sessionfinish, if reference update is required
    out_file = Path(__file__).parent / "worker_logs" / f"{app}_{target}_mips_worker.json"
    out_file.parent.mkdir(parents=True, exist_ok=True)  # create missing dirs
    out_file.write_text(json.dumps({target: {app: mips}}))

    if not update:
        threshold = 0.1 # Allow upto 0.1 mips of variation
        fail_str = ""
        ref_json = Path(__file__).parent / "lib_voice_mips.json"
        with ref_json.open("r") as f:
            ref_data = json.load(f)
        assert target in ref_data and app in ref_data.get(target, {}), (
                                     f"ERROR: App {app} for arch {target} not in reference json. "
                                     "Run test with pytest test_profile_mips.py --update "
                                     "to regenerate the reference json and rst")
        if abs(mips - ref_data[target][app]) > threshold:
            fail_str = (
                        f"ERROR: App {app}, MIPS {mips} off by more than "
                        f"{threshold} MIPS compared to the reference {ref_data[target][app]}.\n"
                        "If this is expected, run the test with 'pytest test_profile_mips.py' --update to update the reference json and rst files.\n"
                        )
            pytest.fail(fail_str)
