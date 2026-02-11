
import pytest
import numpy as np
import py_vs_c_utils as pvc

@pytest.mark.parametrize("y_ch, x_ch, main_ph, shadow_ph", [[1, 1, 10, 0]])
def test_calc_freq_domain_energy(aec_obj, rng, dut_runner):
  fd_frame_len = aec_obj.f_bin_count

  data_len = fd_frame_len * 2 + 1 # for exp

  out_len = 2 # float_s32_t
  in_len = data_len
  input_data = np.array([in_len, out_len], dtype=np.int32)

  test_frames = 1<<10
  ref_energy = np.empty(0, dtype=np.float64)

  for _ in range(test_frames):
    data = pvc.rand_int32_arr(rng, fd_frame_len * 2, 4)
    data_exp = rng.integers(-31, -24, size=1, dtype=np.int32)

    input_data = np.append(input_data, data_exp)
    input_data = np.append(input_data, data)

    data_fl = pvc.int32_to_double(data, data_exp)
    data_fl = data_fl.view(np.complex128)
    data_fl = data_fl.reshape((1, fd_frame_len))

    ref_energy_one = aec_obj.calc_ov_energy(data_fl)

    ref_energy = np.append(ref_energy, ref_energy_one)

  op = dut_runner(input_data)

  dut_energy = pvc.float_s32_arr_to_double(op)

  np.testing.assert_allclose(ref_energy, dut_energy, rtol=0, atol=6e-4)
