
import pytest
import numpy as np
import py_vs_c_utils as pvc
from conftest import gen_bfps

def float64_to_comp128_reshape(data, shape):
  data = data.view(np.complex128)
  data = data.reshape(shape)
  return data

@pytest.mark.parametrize("y_ch, x_ch, main_ph, shadow_ph", [[1, 2, 5, 3]])
def test_calc_Error_and_Y_hat(fdaf_obj, y_ch, x_ch, main_ph, rng, dut_runner):
  f_bin_count = fdaf_obj.f_bin_count

  # all complex and need an exponent (per BFP)
  bfp_size = f_bin_count * 2 + 1  # exponent + complex data (real + imag for each bin)
  Y_len = y_ch * bfp_size
  H_hat_len = y_ch * x_ch * main_ph * bfp_size
  X_len = x_ch * main_ph * bfp_size
  Error_len = Y_len
  Y_hat_len = Y_len

  out_len = Error_len + Y_hat_len
  in_len = Y_len + H_hat_len + X_len
  input_data = np.array([in_len, out_len], dtype=np.int32)

  test_frames = 1<<10
  ref_Error = np.empty(0, dtype=np.float64)
  ref_Y_hat = np.empty(0, dtype=np.float64)

  for _ in range(test_frames):
    # Keep exponent ranges small so the reference doesn't hit huge |X*H| products
    # that make Error ~= -Y_hat due to |Y_hat| >> |Y|.
    Y, Y_fl = gen_bfps(rng, 3, (y_ch, f_bin_count * 2), (-30, -24))
    input_data = np.append(input_data, Y)
    Y_fl = float64_to_comp128_reshape(Y_fl, (y_ch, f_bin_count))

    X, X_fl = gen_bfps(rng, 3, (x_ch, main_ph, f_bin_count * 2), (-30, -24))
    input_data = np.append(input_data, X)
    X_fl = float64_to_comp128_reshape(X_fl, (x_ch, main_ph, f_bin_count))

    # H_hat has 3 dimentions in C and 4 in python
    H_hat, H_hat_fl = gen_bfps(rng, 3, (y_ch, x_ch * main_ph, f_bin_count * 2), (-30, -24))
    input_data = np.append(input_data, H_hat)
    H_hat_fl = float64_to_comp128_reshape(H_hat_fl, (y_ch, x_ch, main_ph, f_bin_count))

    # Set state on the filter where calc_Error_and_Y_hat will access it
    fdaf_obj.main_filter.X_data[:] = X_fl
    fdaf_obj.main_filter.H[:] = H_hat_fl
    fdaf_obj.main_filter.calc_Error_and_Y_hat(Y_fl)

    ref_Y_hat = np.append(ref_Y_hat, fdaf_obj.main_filter.Y_hat.view(np.float64))
    ref_Error = np.append(ref_Error, fdaf_obj.main_filter.Error.view(np.float64))

  op = dut_runner(input_data)

  # Both Y_hat and Error have [y_ch][f_bin_count] dimentions and are complex
  sections = np.cumsum(np.tile([bfp_size, bfp_size], test_frames * y_ch))[:-1].astype(np.int32)
  op_split = np.split(op, sections)

  dut_Y_hat = np.concatenate(op_split[0::2])
  dut_Y_hat = pvc.bfp_s32_arr_to_double(dut_Y_hat, f_bin_count * 2, test_frames)
  dut_Error = np.concatenate(op_split[1::2])
  dut_Error = pvc.bfp_s32_arr_to_double(dut_Error, f_bin_count * 2, test_frames)

  np.testing.assert_allclose(ref_Y_hat, dut_Y_hat, rtol=0, atol=2e-4)
  np.testing.assert_allclose(ref_Error, dut_Error, rtol=0, atol=2e-4)

