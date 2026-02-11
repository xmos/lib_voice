
import pytest
import numpy as np
import py_vs_c_utils as pvc


@pytest.mark.parametrize("y_ch, x_ch, main_ph, shadow_ph", [[1, 4, 10, 0]])
def test_calc_max_ref_energy(aec_obj, x_ch, rng, dut_runner):
  frame_advance = aec_obj.frame_advance

  in_len = x_ch * frame_advance
  out_len = 2  # float_s32_t
  input_data = np.array([in_len, out_len], dtype=np.int32)

  test_frames = 1 << 10
  ref_energy = np.empty(0, dtype=np.float64)

  for _ in range(test_frames):
    frame_int = np.empty((x_ch, frame_advance), dtype=np.int32)
    for ch in range(x_ch):
      frame_int[ch] = pvc.rand_int32_arr(rng, frame_advance, hr_max=12)

    input_data = np.append(input_data, frame_int.ravel())

    far_frame = pvc.int32_to_double(frame_int, np.int32(-31))

    # py_voice returns mean squared power; C returns sum of squares (energy).
    ref_power = aec_obj.get_max_ref_power(far_frame)
    ref_energy_one = ref_power * frame_advance
    ref_energy = np.append(ref_energy, ref_energy_one)

  op = dut_runner(input_data)

  dut_energy = pvc.float_s32_arr_to_double(op)

  np.testing.assert_allclose(ref_energy, dut_energy, rtol=0, atol=1e-7)
