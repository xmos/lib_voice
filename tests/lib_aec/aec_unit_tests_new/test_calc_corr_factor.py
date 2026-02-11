
import pytest
import numpy as np
import py_vs_c_utils as pvc


@pytest.mark.parametrize("y_ch, x_ch, main_ph, shadow_ph", [[1, 1, 10, 0]])
def test_calc_corr_factor(aec_obj, rng, dut_runner):
  frame_advance = aec_obj.frame_advance
  test_fr_len = frame_advance - aec_obj.overlap_width

  y_len = test_fr_len + 1 # for exp
  y_hat_len = y_len

  out_len = 2 # float_s32_t
  in_len = y_len + y_hat_len
  input_data = np.array([in_len, out_len], dtype=np.int32)

  test_frames = 1<<10
  ref_corr = np.empty(0, dtype=np.float64)

  for _ in range(test_frames):
    y= pvc.rand_int32_arr(rng, test_fr_len, 4)
    y_exp = rng.integers(-31, 32, size=1, dtype=np.int32)

    input_data = np.append(input_data, y_exp)
    input_data = np.append(input_data, y)

    y_hat= pvc.rand_int32_arr(rng, test_fr_len, 4)
    y_hat_exp = rng.integers(-31, 32, size=1, dtype=np.int32)

    input_data = np.append(input_data, y_hat_exp)
    input_data = np.append(input_data, y_hat)

    y_fl = pvc.int32_to_double(y, y_exp)
    y_hat_fl = pvc.int32_to_double(y_hat, y_hat_exp)

    start_i = frame_advance
    stop_i = start_i + test_fr_len
    aec_obj.y_data[0][start_i:stop_i] = y_fl
    aec_obj.y_hat[0][start_i:stop_i] = y_hat_fl
    ref_corr_one = aec_obj.calc_corr_factor()

    ref_corr = np.append(ref_corr, ref_corr_one)

  op = dut_runner(input_data)

  dut_corr = pvc.float_s32_arr_to_double(op)

  np.testing.assert_allclose(ref_corr, dut_corr, rtol=0, atol=1e-9)
