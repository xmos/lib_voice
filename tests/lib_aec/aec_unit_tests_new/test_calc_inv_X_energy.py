
import pytest
import numpy as np
import py_vs_c_utils as pvc
from conftest import gen_bfps

@pytest.mark.parametrize("y_ch, x_ch, main_ph, shadow_ph", [[1, 2, 6, 2]])
def test_calc_inv_X_energy(aec_obj, x_ch, rng, dut_runner):
  f_bin_count = aec_obj.f_bin_count

  sigma_xx_len = x_ch * (f_bin_count + 1) # for exp
  X_energy_len = sigma_xx_len
  delta_len = 2 # float_s32_t

  inv_X_energy_len = sigma_xx_len

  in_len = sigma_xx_len + X_energy_len + delta_len + 1 # for is_shadow
  out_len = inv_X_energy_len
  input_data = np.array([in_len, out_len], dtype=np.int32)

  test_frames = 1<<10
  ref_inv_X_energy = np.empty(0, dtype=np.float64)

  for _ in range(test_frames):
    is_shadow = rng.integers(2, size=1, dtype=np.int32)
    input_data = np.append(input_data, is_shadow)

    delta_mant = pvc.rand_int32_arr(rng, size=1)
    delta_exp = rng.integers(-95, -31, size=1, dtype=np.int32)
    input_data = np.append(input_data, [delta_mant, delta_exp])
    delta_fl = np.ldexp(delta_mant, delta_exp)

    sigma_xx, sigma_xx_fl = gen_bfps(rng, 4, (x_ch, f_bin_count), (-31, 31))
    input_data = np.append(input_data, sigma_xx)
    sigma_xx_fl = sigma_xx_fl.reshape((x_ch, f_bin_count))

    X_energy, X_energy_fl = gen_bfps(rng, 4, (x_ch, f_bin_count), (-31, 31))
    input_data = np.append(input_data, X_energy)
    X_energy_fl = X_energy_fl.reshape((x_ch, f_bin_count))

    aec_obj.sigma_xx = sigma_xx_fl
 
    if not is_shadow:
      aec_obj.main_filter.X_energy = X_energy_fl
      aec_obj.main_filter.delta = delta_fl
      inv_X_energy_one = aec_obj.main_filter.calc_inv_x_energy(aec_obj)
    else:
      aec_obj.shadow_filter.X_energy = X_energy_fl
      aec_obj.shadow_filter.delta = delta_fl
      inv_X_energy_one = aec_obj.shadow_filter.calc_inv_x_energy(aec_obj)

    ref_inv_X_energy = np.append(ref_inv_X_energy, inv_X_energy_one.real)
  
  # Extra test, for the case where delta is zero and X_energy is zero
  # currenty handled differently in python and C, hence uncommented..
   
  # test_frames += 1
  # input_data = np.append(input_data, 1) # shadow only
  # input_data = np.append(input_data, [0, -1024]) # zero delta
  # input_data = np.append(input_data, sigma_xx) # same sigma
  # # getting 0/int32max * 2 ** exp array from X_energy
  # (X_energy_exp0, X_energy_exp1) = (X_energy[0], X_energy[1 + f_bin_count])
  # X_energy = np.signbit(X_energy).astype(np.int32) * np.iinfo(np.int32).max
  # X_energy[0] = X_energy_exp0
  # X_energy[1 + f_bin_count] = X_energy_exp1
  # input_data = np.append(input_data, X_energy)
  
  # X_energy_fl[0][:] = pvc.int32_to_double(X_energy[1:f_bin_count + 1], X_energy_exp0)
  # X_energy_fl[1][:] = pvc.int32_to_double(X_energy[f_bin_count + 2:], X_energy_exp1)
  # aec_obj.shadow_filter.X_energy = X_energy_fl
  # # not setting the delta this time letting the API handle it
  # aec_obj.shadow_filter.calc_delta()
  # inv_X_energy_one = aec_obj.shadow_filter.calc_inv_x_energy(aec_obj)

  # ref_inv_X_energy = np.append(ref_inv_X_energy, inv_X_energy_one.real)

  op = dut_runner(input_data)

  dut_inv_X_energy = pvc.bfp_s32_arr_to_double(op, f_bin_count, test_frames * x_ch)

  np.testing.assert_allclose(ref_inv_X_energy, dut_inv_X_energy, rtol=1e-3, atol=1e-6)
