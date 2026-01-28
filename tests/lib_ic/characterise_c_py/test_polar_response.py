# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

from get_polar_response import get_polar_response

ANGLE_ROI = 360
ANGLE_STEP_SIZE = 120
RT60 = 0.3
NOISE_BAND = 8000
NOISE_LEVEL = -20


def test_compare_polar_reponse():
    angles, results = get_polar_response("atten_pvc",
                                         ANGLE_ROI,
                                         ANGLE_STEP_SIZE,
                                         NOISE_BAND,
                                         NOISE_LEVEL,
                                         RT60)
    for (i, atten_py, atten_c) in zip(angles, results[0], results[1]):
        print(f"Angle: {i}, PY {atten_py}, C {atten_c}")
        assert abs(atten_py - atten_c) < 1, "Angle: {}, PY {}, C {}".format(i, atten_py, atten_c)
