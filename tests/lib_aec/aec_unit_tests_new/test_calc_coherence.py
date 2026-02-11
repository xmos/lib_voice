
import pytest
import numpy as np
import py_vs_c_utils as pvc


@pytest.mark.parametrize("y_ch, x_ch, main_ph, shadow_ph", [[1, 1, 9, 0]])
def test_calc_coherence(aec_obj, rng, dut_runner):
  frame_advance = aec_obj.frame_advance

  y_len = frame_advance + 1 # for exp
  y_hat_len = y_len

  out_len = 2 * 2 # 2 x float_s32_t
  in_len = y_len + y_hat_len
  input_data = np.array([in_len, out_len], dtype=np.int32)

  test_frames = 1<<10
  ref_coh = np.empty(0, dtype=np.float64)
  ref_coh_slow = np.empty(0, dtype=np.float64)

  for _ in range(test_frames):
    y = pvc.rand_int32_arr(rng, frame_advance, 4)
    y_exp = rng.integers(-31, 32, size=1, dtype=np.int32)

    input_data = np.append(input_data, y_exp)
    input_data = np.append(input_data, y)

    y_hat = pvc.rand_int32_arr(rng, frame_advance, 4)
    y_hat_exp = rng.integers(-31, 32, size=1, dtype=np.int32)

    input_data = np.append(input_data, y_hat_exp)
    input_data = np.append(input_data, y_hat)

    y_fl = pvc.int32_to_double(y, y_exp)
    y_hat_fl = pvc.int32_to_double(y_hat, y_hat_exp)

    start_i = frame_advance
    stop_i = frame_advance * 2
    aec_obj.y_data[0][start_i:stop_i] = y_fl
    aec_obj.y_hat[0][start_i:stop_i] = y_hat_fl
    aec_obj.calc_coherence()

    ref_coh = np.append(ref_coh, aec_obj.coh)
    ref_coh_slow = np.append(ref_coh_slow, aec_obj.coh_slow)

  op = dut_runner(input_data)

  sections = np.cumsum(np.tile([2, 2], test_frames))[:-1].astype(np.int32)
  op_split = np.split(op, sections)

  dut_coh = np.concatenate(op_split[0::2])
  dut_coh = pvc.float_s32_arr_to_double(dut_coh)
  dut_coh_slow = np.concatenate(op_split[1::2])
  dut_coh_slow = pvc.float_s32_arr_to_double(dut_coh_slow)

  np.testing.assert_allclose(ref_coh, dut_coh, rtol=0, atol=2e-1)
  np.testing.assert_allclose(ref_coh_slow, dut_coh_slow, rtol=0, atol=1e-3)
