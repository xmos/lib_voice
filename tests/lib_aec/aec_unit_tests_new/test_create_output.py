
import pytest
import numpy as np
import py_vs_c_utils as pvc


@pytest.mark.parametrize("y_ch, x_ch, main_ph, shadow_ph", [[1, 1, 9, 9]])
def test_create_output(aec_obj, rng, dut_runner):
  frame_advance = aec_obj.frame_advance
  proc_frame_length = aec_obj.proc_frame_length
  overlap_width = aec_obj.overlap_width

  in_len = 1 + proc_frame_length # for exp
  out_len = frame_advance + (1 + overlap_width) + (1 + proc_frame_length)
  input_data = np.array([in_len, out_len], dtype=np.int32)

  test_frames = 1 << 10
  ref_out = np.empty(0, dtype=np.float64)
  ref_ov = np.empty(0, dtype=np.float64)
  ref_err = np.empty(0, dtype=np.float64)

  # In this test we have to set the same exponent for all the tests as
  # doing otherwise will make C to saturate (cause it has to output in q0.31)
  # whereas python does not do so.
  error_exp = rng.integers(-31, -27, size=1, dtype=np.int32)

  for _ in range(test_frames):
    err_int = pvc.rand_int32_arr(rng, proc_frame_length, hr_max=4)

    input_data = np.append(input_data, error_exp)
    input_data = np.append(input_data, err_int)

    err_fl = pvc.int32_to_double(err_int, error_exp).reshape(1, proc_frame_length)

    # Apply the same time-domain modifications as C (zero + window) before overlap-add.
    # window_error mutates err_fl in-place.
    aec_obj.window_error(err_fl)
    # aec.created output would overwrite the error memory so coppying the array before calling the API
    err_win_fl = err_fl.copy()
    out_fl = aec_obj.create_output(err_fl)

    ref_out = np.append(ref_out, out_fl.ravel())
    ref_ov = np.append(ref_ov, aec_obj.overlap.ravel())
    ref_err = np.append(ref_err, err_win_fl.ravel())

  op = dut_runner(input_data)
  op = op.reshape(test_frames, out_len)

  dut_out_int = op[:, :frame_advance].ravel().astype(np.int32)
  dut_out = pvc.int32_to_double(dut_out_int, np.int32(-31))

  dut_ov = pvc.bfp_s32_arr_to_double(op[:, frame_advance: frame_advance + 1 + overlap_width].ravel(), overlap_width, test_frames)

  dut_err = pvc.bfp_s32_arr_to_double(op[:, frame_advance + 1 + overlap_width:].ravel(), proc_frame_length, test_frames)

  np.testing.assert_allclose(ref_out, dut_out, rtol=0, atol=3e-7)
  np.testing.assert_allclose(ref_ov, dut_ov, rtol=0, atol=3e-7)
  np.testing.assert_allclose(ref_err, dut_err, rtol=0, atol=3e-7)
