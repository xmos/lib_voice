# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import shutil
import sys
import xmos_ai_tools.runtime as rt
from cffi import FFI
import subprocess
from pathlib import Path

from extract_state import extract_pre_defs

REPO_ROOT = Path(__file__).parents[3]
MODULE_ROOT = REPO_ROOT / "lib_voice"
XCORE_MATH = REPO_ROOT.parent / "lib_xcore_math"
LIBS_BUILD_DIR = Path(__file__).parent/ "build_libs_x86"

# TFLite Micro configuration
TFLITE_MICRO_ROOT = Path(rt.__file__).parent
TFLITE_MICRO_LIB_DIR = TFLITE_MICRO_ROOT / "lib"
TFLITE_MICRO_INCLUDE = TFLITE_MICRO_ROOT / "include"
TFLITE_MICRO_LIB = "host_xtflitemicro"  # use the host platform

FLAGS = [
    '-fPIC',
    '-DTF_LITE_STATIC_MEMORY',           # Define TF_LITE_STATIC_MEMORY
    '-DTF_LITE_STRIP_ERROR_STRINGS',     # Define TF_LITE_STRIP_ERROR_STRINGS
]

INCLUDE_DIRS=[
    str(MODULE_ROOT / "src" / "ic"),
    str(MODULE_ROOT / "api" / "ic"),
    str(MODULE_ROOT / "api" / "vnr"),
    str(MODULE_ROOT / "src" / "vnr"),
    str(MODULE_ROOT / "src" / "vnr" / "model"),
    str(XCORE_MATH / "lib_xcore_math" / "api"),
    str(TFLITE_MICRO_INCLUDE)
]

LIBRARY_DIRS = [
    str(LIBS_BUILD_DIR / "lib_voice"),
    str(LIBS_BUILD_DIR / "lib_xcore_math"),
    str(TFLITE_MICRO_LIB_DIR)
]

LIBRARIES = [
    'fwk_voice_module_lib_ic',
    'fwk_voice_module_lib_vnr',
    'lib_xcore_math',
    TFLITE_MICRO_LIB,
    'm',
    'stdc++'
] # on Unix, link with the math library. Linking order is important here for gcc compile on Linux!

SRCS = f"../ic_test.c".split()
ffibuilder = FFI()

#Extract all defines and state from lib_ic programatically
predefs = extract_pre_defs()
predefs = predefs.replace("sizeof(uint64_t)", "8")
print(predefs)
# Contains all the C defs visible from Python
ffibuilder.cdef(
predefs +
"""
    void test_init(void);
    ic_state_t test_get_state(void);
    void test_filter(int32_t y_data[IC_FRAME_ADVANCE], int32_t x_data[IC_FRAME_ADVANCE], int32_t output[IC_FRAME_ADVANCE]);
    void test_adapt(float_s32_t vnr);
""".replace("IC_FRAME_ADVANCE", "240")
)

# Contains the C source necessary to allow the cdefs to work
ffibuilder.set_source("ic_test_py",  # name of the output C extension
"""
    #include "ic.h"
    void test_init(void);
    ic_state_t test_get_state(void);
    void test_filter(int32_t y_data[IC_FRAME_ADVANCE], int32_t x_data[IC_FRAME_ADVANCE], int32_t output[IC_FRAME_ADVANCE]);
    void test_adapt(float_s32_t vnr);
""",
    sources=SRCS,
    library_dirs=LIBRARY_DIRS,
    libraries=LIBRARIES,
    extra_compile_args=FLAGS,
    include_dirs=INCLUDE_DIRS)

if __name__ == "__main__":
    subprocess.run(["cmake", "-B", str(LIBS_BUILD_DIR), "-G", "Unix Makefiles"], check=True)
    subprocess.run(["xmake", "-C", str(LIBS_BUILD_DIR), "-j"], check=True)

    ffibuilder.compile(tmpdir='build', target='ic_test_py.*', verbose=True)
    #Darwin hack https://stackoverflow.com/questions/2488016/how-to-make-python-load-dylib-on-osx
    if sys.platform == "darwin":
        shutil.copyfile("build/ic_test_py.dylib", "build/ic_test_py.so")


