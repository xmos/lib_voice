# Copyright 2021-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
from builtins import str
from builtins import range
import os.path
from pathlib import Path
import configparser
import numpy as np
import scipy.io.wavfile
import scipy.signal.windows
from audio_generation import (get_filenames, get_magnitude,
                              get_suppressed_magnitude, db, get_h)


def files_exist(*args):
    for filename in args:
        if not os.path.isfile(filename):
            return False
    return True


def read_config(testname):
    parser = configparser.ConfigParser()
    parser.read(Path(__file__).parent / "test_config.cfg")
    cfg = {}
    cfg['settle_time'] = parser.getint(testname, "settle_time")
    cfg['start_fft'] = parser.getint(testname, "start_fft")
    cfg['end_fft'] = parser.getint(testname, "end_fft")
    cfg['ignore_exclusion'] = parser.getboolean(testname, "ignore_exclusion")
    cfg['headroom'] = [int(x) for x in parser.get(testname, "headroom").split(',')]
    cfg['echo'] = [x.strip() for x in parser.get(testname, "echo").split(',')]
    cfg['reference'] = [x.strip() for x in parser.get(testname, "reference").split(',')]
    try:
        frequencies = [int(x) for x in parser.get(testname, "frequencies").split(',')]
        cfg['frequencies'] = frequencies
    except configparser.NoOptionError:
        pass
    return cfg


def read_wav(filename):
    rate, data = scipy.io.wavfile.read(filename)
    return data.astype(float) / np.iinfo(data.dtype).max


def get_excluded_tests():
    excluded_tests = []
    if os.environ.get("FULL_TEST") == "0":
        filename = Path(__file__).parent / "excluded_tests_quick.txt"
    else:
        filename = Path(__file__).parent / "excluded_tests.txt"

    with open(filename, 'r') as f:
        for line in f.readlines():
            line = line.strip()
            excluded_tests.append(line)
    return list(set(excluded_tests))


def get_test_instances(testname, in_dir, out_dir):
    """ Gets all generated tests by checking test_config.cfg and the files
        present in the in_dir.

        Any tests in excluded_tests.txt will not be included."""
    tests = []
    excluded_tests = get_excluded_tests()
    cfg = read_config(testname)
    for headroom in cfg['headroom']:
        for echo_type in cfg['echo']:
            for ref_type in cfg['reference']:
                test_id = ",".join([testname, echo_type, ref_type,
                                    str(headroom)])
                in_filename, ref_filename, out_filename\
                    = get_filenames(testname, echo_type, ref_type, headroom)
                in_filename = os.path.join(in_dir, in_filename + ".wav")
                ref_filename = os.path.join(in_dir, ref_filename + ".wav")
                out_filename = os.path.join(out_dir,
                                               out_filename + ".wav")
                test_dict = {'id' : test_id,
                             'test_type' : testname,
                             'settle_time' : cfg['settle_time'],
                             'headroom' : headroom,
                             'echo' : echo_type,
                             'reference' : ref_type,
                             'in_filename' : in_filename,
                             'ref_filename' : ref_filename,
                             'out_filename' : out_filename}
                if files_exist(in_filename, ref_filename):
                    tests.append(test_dict)
    return tests


def get_section(testid, sections):
    best_section = 'DEFAULT'
    best_precision = np.iinfo(np.int32).min
    testid = testid.split(',')
    for section in sections:
        section = section.split(',')
        precision = -len([x for x in section if x == '*'])
        match = True
        for i in range(len(section)):
            if section[i] == testid[i] or section[i] == '*':
                continue
            match = False
        if match and precision > best_precision:
            best_precision = precision
            best_section = ','.join(section)
    return best_section


def get_criteria(testid):
    parser = configparser.ConfigParser()
    parser.read(Path(__file__).parent / 'criteria.cfg')
    criteria = {}
    section = get_section(testid, parser.sections())
    for key, val in parser.items(section):
        criteria[key] = val
    return criteria


def get_h_hat_impulse_response(h_hat, y_channel, x_channel):
    """Gets the impulse response of h_hat.

    h_hat is an array internal to the aec with a shape as follows:
    (y_channel_count, x_channel_count, max_phase_count, frame_advance)
    The filter is stored in the time domain, so each phase is already the impulse response for
    that phase; no transform is needed to recover it.

    Args:
        h_hat: h_hat array
        y_channel: y_channel to plot
        x_channel: x_channel to plot

    Returns:
        Impulse response of h_hat for channel pair (y_channel, x_channel)
    """

    max_phase_count = h_hat.shape[2]
    frame_advance   = h_hat.shape[3]
    h_hat_ir = np.zeros((max_phase_count * frame_advance,))

    for phase in range(max_phase_count):
        start   = frame_advance *  phase
        end     = frame_advance * (phase + 1)
        h_hat_ir[start:end] = h_hat[y_channel][x_channel][phase]

    return h_hat_ir


def power_db(x):
    return 10 * np.log10(max(x, 1e-30))


def get_misalignment(h_hat_ir, h):
    """Normalised misalignment of an echo path estimate, in dB:
    10log10(||h_hat_ir - h||^2 / ||h||^2), over all taps of the longer of the two.

    For a white reference this is minus the ERLE the estimate can give with no near-end, so
    -misalignment is the ERLE ceiling set by the filter alone."""
    err = np.zeros(max(len(h_hat_ir), len(h)))
    err[:len(h_hat_ir)] += h_hat_ir
    err[:len(h)] -= h
    return power_db(np.sum(err ** 2) / np.sum(h ** 2))


def get_output_impulse_response(audio_ref, audio_out, impulse_index, length, h,
                                max_lag=240):
    """Measures h_hat through the real datapath, from the AEC output after an impulse injected
    into the reference only, with adaptation frozen: out = mic - h_hat*ref, so -out/impulse is
    h_hat plus the near-end. The AEC output lags its input by a few samples; that lag (at most
    max_lag) is found by correlating with the true echo path h."""
    amplitude = audio_ref[impulse_index]
    segment = -audio_out[impulse_index:impulse_index + max_lag + length] / amplitude
    lag = int(np.argmax(np.correlate(segment[:max_lag + len(h)], h, 'valid')))
    return segment[lag:lag + length]


def get_impulse_response_metrics(h_hat_ir, echo_type, audio_in, audio_ref, audio_out,
                                 settle_time, Fs=16000):
    """Numeric results for the impulse response tests, against the true echo path.

    Returns a dict with:
        misalignment: of the h_hat dump taken when adaptation stops (dB)
        erle_ceiling: -misalignment, the ERLE that filter precision alone allows (dB)
        erle: measured ERLE, mic vs AEC output over the 3 s before adaptation stops (dB)
        out_misalignment: of h_hat measured at the AEC output from the impulse injected into the
            reference 1 s after adaptation stops (dB)
    """
    h = get_h(echo_type, normalise=False)
    misalignment = get_misalignment(h_hat_ir, h)

    start = (settle_time - 3) * Fs
    end = settle_time * Fs
    erle = power_db(np.mean(audio_in[start:end] ** 2) / np.mean(audio_out[start:end] ** 2))

    out_ir = get_output_impulse_response(audio_ref, audio_out, (settle_time + 1) * Fs,
                                         len(h_hat_ir), h)
    return {'misalignment': misalignment,
            'erle_ceiling': -misalignment,
            'erle': erle,
            'out_misalignment': get_misalignment(out_ir, h),
            'out_ir': out_ir}


def check_impulse_response(metrics, criteria, check_output=True):
    """Checks the impulse response metrics against criteria['ir_max_misalignment'] (dB).

    The h_hat dump is always checked; the output-measured IR only if check_output is set, as it
    is limited by the near-end noise when the probe impulse is small.

    Returns (passed, report), where report is a printable summary of the metrics."""
    max_misalignment = float(criteria['ir_max_misalignment'])
    checks = [('misalignment', 'h_hat misalignment')]
    if check_output:
        checks.append(('out_misalignment', 'output-measured misalignment'))

    lines = ["h_hat misalignment: %.1f dB" % metrics['misalignment'],
             "ERLE ceiling (-misalignment): %.1f dB" % metrics['erle_ceiling'],
             "measured ERLE (3 s before adaptation stops): %.1f dB" % metrics['erle'],
             "output-measured misalignment: %.1f dB%s" % (metrics['out_misalignment'],
                 "" if check_output else " (not checked, near-end noise limited)")]
    passed = True
    for key, name in checks:
        if metrics[key] > max_misalignment:
            lines.append("Check failed! %s %.1f dB (> %.1f dB)"
                         % (name, metrics[key], max_misalignment))
            passed = False
    if passed:
        lines.append("Check passed! (misalignment <= %.1f dB)" % max_misalignment)
    return passed, "\n".join(lines) + "\n"


def check_aec_output(audio_in, audio_ref, audio_out, start_s, end_s, criteria,
                     frequencies, Fs=16000, debug=True):
    success = True
    start = Fs * start_s
    end = Fs * end_s
    window = scipy.signal.windows.hann(end - start, sym=True)
    In = np.abs(np.fft.rfft(audio_in[start:end] * window))
    Ref = np.abs(np.fft.rfft(audio_ref[start:end] * window))
    Out = np.abs(np.fft.rfft(audio_out[start:end] * window))
    # Check for near-end frequencies
    for f in frequencies:
        in_mag = get_magnitude(f, In, Fs, 10, normalise=True)
        out_mag = get_magnitude(f, Out, Fs, 10, normalise=True)
        db_out = db(out_mag, in_mag)
        suppression_max = int(criteria['near_end_max_suppression'])
        if db_out < suppression_max:
            print("Check failed! Near-end Frequency: %d, db: %f (< %ddB)"\
                  % (f, db_out, suppression_max))
            success = False
        if debug:
            print("freq: %d, in: %f, out: %f, db: %f"\
                  % (f, in_mag, out_mag, db_out))
    # Check magnitude of suppressed frequencies
    band_min = int(criteria['suppression_band_min'])
    band_max = int(criteria['suppression_band_max'])
    max_suppressed_magnitude, freq = get_suppressed_magnitude(frequencies, Out,
                                                              Fs, 10,
                                                              band_min=band_min,
                                                              band_max=band_max)
    suppression_min = int(criteria['far_end_min_suppression'])
    db_suppressed = db(max_suppressed_magnitude, np.max(In))
    if db_suppressed > suppression_min:
        print("Check failed! Suppression at freq %d is %fdB (> %ddB)"\
                % (freq, db_suppressed, suppression_min))
        success = False
    else:
        print("Suppression check passed.")
    if success:
        print("Check passed!")
    return success
