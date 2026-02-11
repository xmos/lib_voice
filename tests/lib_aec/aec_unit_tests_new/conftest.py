# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import pytest
from pathlib import Path
import py_voice.modules.aec as aec
import py_voice.config.config as pv_config
import py_voice.core.fdaf_controller as fdc
import py_voice
import numpy as np
import py_vs_c_utils as pvc
from run_dut import run_dut

PY_VOICE_ROOT = Path(py_voice.__file__).resolve().parent
default_conf_path = PY_VOICE_ROOT / "config" / "defaults.json"
default_conf = pv_config.get_config_dict(default_conf_path)
bin_dir_path = Path(__file__).parent / "bin"

def gen_bfp(rng, len, hr_max, exps=(-31, 31)):
  data = pvc.rand_int32_arr(rng, len, hr_max)
  exp = rng.integers(exps[0], exps[1] + 1, size=1, dtype=np.int32)
  data_fl64 = pvc.int32_to_double(data, exp)
  data_int = np.concatenate([exp, data])
  return data_int, data_fl64

def gen_bfps(rng, hr_max, shape, exps=(-31, 31)):
  bfp_len = shape[-1]
  bfp_num = 1
  data_int = np.empty(0, dtype=np.int32)
  data_fl64 = np.empty(0, dtype=np.float64)
  for dim in range(len(shape) - 1):
    bfp_num *= shape[dim]

  for _ in range(bfp_num):
    bfp_int, arr_fl64 = gen_bfp(rng, bfp_len, hr_max, exps)
    data_int = np.append(data_int, bfp_int)
    data_fl64 = np.append(data_fl64, arr_fl64)
  
  return data_int, data_fl64

def set_aec_conf(y_ch, x_ch, main_ph, shadow_ph):
    test_conf = default_conf
    test_conf["general"]["modules"] = ["aec"]
    test_conf["general"]["input_channel_count"] = y_ch + x_ch
    test_conf["general"]["output_channel_count"] = y_ch
    test_conf["aec"]["input_channel_count"] = y_ch + x_ch
    test_conf["aec"]["output_channel_count"] = y_ch
    test_conf["aec"]["y_channel_count"] = y_ch
    test_conf["aec"]["x_channel_count"] = x_ch
    test_conf["aec"]["phases"] = main_ph
    test_conf["aec"]["phases_shadow"] = shadow_ph

    return test_conf

@pytest.fixture
def aec_obj(y_ch, x_ch, main_ph, shadow_ph):
    test_conf = set_aec_conf(y_ch, x_ch, main_ph, shadow_ph)

    return aec.aec(test_conf)

@pytest.fixture
def fdaf_obj(y_ch, x_ch, main_ph, shadow_ph):
    test_conf = set_aec_conf(y_ch, x_ch, main_ph, shadow_ph)

    return fdc.fdaf_controller(test_conf, "aec")

@pytest.fixture
def dut_runner(request, target):
    exe_path = bin_dir_path / request.node.originalname / f"aec_unit_tests_new_{request.node.originalname}"

    def _run_dut(input_bin):
        op, _ = run_dut(input_bin, exe_path, target)
        return op

    return _run_dut

@pytest.fixture
def rng():
    return np.random.default_rng(1243)

def pytest_generate_tests(metafunc):
    if "target" in metafunc.fixturenames:
        metafunc.parametrize("target", [
          'native',
          'xs3a'
          ])
