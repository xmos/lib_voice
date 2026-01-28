# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import warnings
import audio_generation
import pytest
import numpy as np
import scipy
import filters
from py_voice.modules import ic
from py_voice.config import config
from pathlib import Path
import py_vs_c_utils as pvc
from run_dut import run_dut
import soundfile as sf

output_folder = Path(__file__).parent / "output_wavs"
ap_config_file = Path(__file__).parents[2] / "shared" / "config" / "ic_conf_no_adapt_control.json"
ap_conf = config.get_config_dict(ap_config_file)

xe_path = Path(__file__).parent / "bin" / "test_ic_spec"

sample_rate = ap_conf["general"]["fs"]
proc_frame_length = ap_conf["general"]["proc_frame_length"]
frame_advance = ap_conf["general"]["frame_advance"]
y_chan_delay = ap_conf["ic"]["y_channel_delay"]

class TestCase(object):
    def __init__(self, name, h_x, h_y, aud_x=None, aud_y=None, dont_check=[], invert_check=[]):
        self.name = name
        self.h_x = h_x
        self.h_y = h_y
        self.aud_x = aud_x
        self.aud_y = aud_y
        self._dont_check = dont_check
        self._invert_check = invert_check

        if aud_x is None:
            self.aud_x = audio_generation.get_noise(duration=10, db=-20)
        if aud_y is None:
            self.aud_y = audio_generation.get_noise(duration=10, db=-20)


    def get_test_name(self):
        return self.name.lower().replace(' ', '-')


    def __getattr__(self, name):
        if "do_check" == name[:len("do_check")]:
            check = name[len("do_check_"):]
            return not check in self._dont_check
        if "invert_check" == name[:len("invert_check")]:
            check = name[len("invert_check_"):]
            return check in self._invert_check
        raise AttributeError


# IC Spec
class ICSpec(object):
    # Minimum time the IC should take to converge
    convergence_time = 3

    # The dB suppression required for convergence
    db_suppression = 15

    # The amount of noise the IC can add to the input before it's considered
    # unstable
    db_max_noise_produced = 2

    # Expected delay (samples) on the output
    #
    # Unsure exactly why, but the output has an extra (proc_frame_len % frame_advance)
    # sample delay.
    expected_delay = y_chan_delay + (proc_frame_length % frame_advance)

test_vectors = [
#    TestCase('Identical Mics', filters.Identity(), filters.Identity()),
    TestCase('Uncorrelated Mics', filters.Identity(), filters.Identity(),
             aud_x=audio_generation.get_noise(duration=10, db=-20, seed=0),
             aud_y=audio_generation.get_noise(duration=10, db=-20, seed=1),
             invert_check=['convergence'], dont_check=['stability']),
    TestCase('Impulse at 40 samples', filters.Identity(), filters.OneImpulse(40)),
    TestCase('Impulse at minus 20 samples', filters.OneImpulse(20), filters.Identity()),
#    TestCase('Diffuse noise', filters.Diffuse(0), filters.Diffuse(1),
#             invert_check=['convergence']),
    TestCase('Short Echo', filters.Identity(), filters.ShortEcho()),
#    TestCase('Zero at 5', filters.Identity(), filters.ZeroAt(5)),
# This test case has been disabled, for details see issue 320 in fwk_voice
#    TestCase('Moving source', filters.Identity(), filters.MovingSource(move_frequency=1.5),
#             dont_check=['stability']),
]


def process_py(input_data):

    ic_obj = ic.ic(ap_conf)
    output_data, _ = ic_obj.process_array(input_data)

    return np.reshape(output_data, output_data.shape[1])


def process_c(input_data, xe_name):
    input_data = pvc.float_to_int32(input_data)

    assert input_data.ndim == 2
    assert input_data.shape[0] == 2

    input_data = pvc.interleave_channel_frames(input_data, frame_advance)

    output_data, _ = run_dut(input_data, xe_name, "xs3a")

    return pvc.int32_to_float(output_data)


@pytest.fixture
def test_input(request):
    test_case = request.param
    test_name = test_case.get_test_name()
    # Generate Audio
    audio_x, audio_y = filters.convolve(test_case.aud_x, test_case.aud_y,
                                        test_case.h_x, test_case.h_y)
    # Last two channels are not used
    combined_data = np.vstack((audio_y, audio_x))
    # crop to have full frames
    inx = combined_data.shape[1] // frame_advance * frame_advance
    combined_data = combined_data[:, :inx]
    if np.max(np.abs(audio_x)) > 1:
        warnings.warn("{}: max(abs(Mic 1)) == {}".format(test_name, np.max(np.abs(audio_x))))
    if np.max(np.abs(audio_y)) > 1:
        warnings.warn("{}: max(abs(Mic 0)) == {}".format(test_name, np.max(np.abs(audio_y))))
    # Write the input audio to file
    #input_32bit = awu.convert_to_32_bit(combined_data)
    return (test_case, combined_data)


def process_audio(model, input_audio, test_name):
    if model == 'py':
        output_file = output_folder / f"out_{test_name}_py.wav"
        output_data = process_py(input_audio)
    elif model == "c":
        output_file = output_folder / f"out_{test_name}_c.wav"
        output_data = process_c(input_audio, xe_path)
    else:
        assert 0, f"model {model} not supported"

    sf.write(output_file, output_data, sample_rate)
    return output_data


def rms(a):
    return np.sqrt(np.mean(np.square(a)))


def get_suppression_arr(input_audio, output_audio):
    """ Gets the dB suppression at each frame """

    if input_audio.dtype == np.int32:
        input_audio = np.asarray(input_audio / float(np.iinfo(np.int32).max), dtype=float)
    if output_audio.dtype == np.int32:
        output_audio = np.asarray(output_audio / float(np.iinfo(np.int32).max), dtype=float)

    num_frames = int(input_audio.shape[1] // frame_advance)
    suppression_arr = np.zeros(num_frames)

    for i in np.arange(0, num_frames*frame_advance, frame_advance):
        i = int(i)
        in_rms = rms(input_audio[0][i:i+frame_advance])

        if len(output_audio.shape) > 1:
            out_rms = rms(output_audio[0, i:i+frame_advance])
        else:
            out_rms = rms(output_audio[i:i+frame_advance])

        if in_rms == 0 or out_rms == 0:
            suppression_db = 0.0
        elif i - frame_advance < ICSpec.expected_delay:
            suppression_db = 0.0
        else:
            suppression_db = 20 * np.log10(in_rms / out_rms)

        suppression_arr[int(i//frame_advance)] = suppression_db
    return suppression_arr


def check_convergence(record_property, test_case, suppression_arr):
    """ Checks the convergence time is less than the spec """

    suppressed_frames = np.argwhere(suppression_arr > ICSpec.db_suppression)
    n_frames_ave = 10
    final_supression = np.average(suppression_arr[-1-n_frames_ave:-1])
    convergence_frame = np.min(suppressed_frames) if suppressed_frames.size != 0 else suppression_arr.size
    convergence_time = convergence_frame * frame_advance / float(sample_rate)
    check = convergence_time < ICSpec.convergence_time

    record_property('Convergence time', str(convergence_time))
    record_property('Final supression dB', str(final_supression))

    # Invert the check if the test vector shouldn't converge
    if not test_case.do_check_convergence:
        return True
    if test_case.invert_check_convergence:
        check = not check
    return check


def check_stability(record_property, test_case, suppression_arr):
    """ Checks that the IC never adds noise to the output """

    max_db_added = -np.min(suppression_arr)
    check = max_db_added < ICSpec.db_max_noise_produced
    record_property('Stable', str(check))
    record_property('Max dB added', str(max_db_added))
    if not test_case.do_check_stability:
        return True
    if test_case.invert_check_stability:
        check = not check
    return check


def check_delay(record_property, test_case, input_audio, output_audio):
    """ Verify that the IC is correctly delaying the input

    If increasing delay, make sure to increase correlation window as well
    """

    if input_audio.dtype == np.int32:
        input_audio = np.asarray(input_audio, dtype=float) / np.iinfo(np.int32).max
    if output_audio.dtype == np.int32:
        output_audio = np.asarray(output_audio, dtype=float) / np.iinfo(np.int32).max

    frame_in = input_audio[0][:frame_advance * 8]

    if len(output_audio.shape) > 1:
        frame_out = output_audio[0, :frame_advance * 8]
    else:
        frame_out = output_audio[:frame_advance * 8]

    corr = scipy.signal.correlate(frame_in, frame_out, mode='same')
    delay = frame_advance * 4 - np.argmax(corr)

    record_property("Delay", str(delay))
    record_property("Expected Delay", str(ICSpec.expected_delay))

    check = (delay == ICSpec.expected_delay)
    if not test_case.do_check_delay:
        return True
    if test_case.invert_check_delay:
        check = not check
    return check


@pytest.mark.parametrize('test_input', test_vectors, indirect=True)
@pytest.mark.parametrize('model', ["py", "c"])
def test_all(test_input, model, record_property):
    test_case, input_audio = test_input
    print("\n{}: {}\n".format(test_case.name, model))
    output_folder.mkdir(exist_ok=True)

    output_audio = process_audio(model, input_audio, test_case.get_test_name())
    suppression_arr = get_suppression_arr(input_audio, output_audio)

    record_property('Test name', test_case.get_test_name())
    record_property('Model', model)
    record_property('suppression_arr', np.array_repr(suppression_arr))

    # Run checks
    converged = check_convergence(record_property, test_case, suppression_arr)
    stable = check_stability(record_property, test_case, suppression_arr)
    delayed = check_delay(record_property, test_case, input_audio, output_audio)

    # Assert checks
    criteria = [converged, stable, delayed]
    assert np.all(criteria), " and ".join([str(c) for c in criteria])

