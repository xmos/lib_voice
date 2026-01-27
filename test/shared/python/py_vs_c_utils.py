# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import numpy as np
import soundfile as sf
from pathlib import Path
import sys

# Turn a float32 from C into an np float scalar
def float_s32_to_float(float_s32):
    return np.ldexp(float_s32.mant, float_s32.exp)

# turn an int32 np array (Q1.31) into float
def int32_to_float(array_int32):
    array_float = np.array(array_int32).astype(np.float64) / (2**31)
    return array_float

# turn an uint8 np array (Q0.8) into float
def uint8_to_float(array_uint8):
    array_float = np.array(array_uint8).astype(np.float64) / (2**8)
    return array_float

# turn a float into an int32 np array (Q1.31)
def float_to_int32(array_float):
    if np.any(np.array(array_float) * (2**31) > np.iinfo(np.int32).max) or np.any(np.array(array_float) * (2**31) < np.iinfo(np.int32).min):
        raise ValueError("float_to_int32: value out of int32 range after scaling to Q31")
    array_int32 = np.clip((np.array(array_float) * (2**31)), np.iinfo(np.int32).min, np.iinfo(np.uint32).max).astype(np.int32)
    return array_int32

# turn a float np array into uint8 (Q0.8)
def float_to_uint8(array_float):
    array_uint8 = np.clip((np.array(array_float) * (2**8)), np.iinfo(np.uint8).min, np.iinfo(np.uint8).max).astype(np.uint8)
    return array_uint8

def double_to_int32(x, exp):
    y = x.astype(np.float64) * (2.0 ** -exp)
    y = y.astype(np.int32)
    return y

def int32_to_double(x, exp):
    y = x.astype(np.float64) * (2.0 ** exp)
    return y

# Convert the flat array, representing float_s32_t
# of form: mantissa (i32), exponent (i32)
# to the np.float64 array
def float_s32_arr_to_double(flat_data):
    mant = flat_data[0::2].astype(np.float64)
    exp = flat_data[1::2]
    ref = mant * (2.0 ** exp)
    return ref

# Convert the flat array, representing bfp_s32_t
# of form: exponent (i32), data array (i32)
# to the np.float64 array
def bfp_s32_arr_to_double(flat_data, bfp_len, num_frames):
    # Do cumulative sum to get indexes for exponents and data array starts
    sections = np.cumsum(np.tile([1, bfp_len], num_frames))[:-1].astype(np.int32)
    split = np.split(flat_data, sections)

    exps = split[0::2]
    manths = split[1::2]

    assert len(exps) == len(manths) == num_frames

    out = np.zeros((num_frames * bfp_len), dtype=np.float64)
    for i in range(num_frames):
        indx = bfp_len * i
        out[indx : indx + bfp_len] = int32_to_double(manths[i], exps[i])

    return out

# Convert a 2d array of data into a flat array, in the form of:
# ch0[0 : frame_len - 1] : ch1[0 : frame_len - 1] :
# ch0[frame_len : 2 * frame_len - 1] : ch1[frame_len : 2 * frame_len - 1] ...
# data has to be [channel, sample] and the samples have to be multiple of frame_len
def interleave_channel_frames(data, frame_len):
    assert data.ndim == 2
    assert data.shape[1] % frame_len == 0
    n_chans = data.shape[0]
    n_frames = data.shape[1] // frame_len
    # reshape to split into frames
    frames = data.reshape(n_chans, n_frames, frame_len)
    # transpose to interleave (n_frames, n_chans, frame_len), flatten
    data = frames.transpose(1, 0, 2).ravel()
    return data

def get_closeness_metric(ref, dut):
    data = np.zeros((2, len(ref)))
    data[0,:] = ref
    data[1,:] = dut
    arith_closeness, geo_closeness, _, _ = pcm_closeness_metric(data, verbose=False)
    return arith_closeness, geo_closeness

# compare a two channel wav file and quantify how close they are
# Any file that is 1 sample out in delay will show low results of 0.20 or worse
# Arithmetic closeness is more sensitive than geo_closeness
# Anything in the 0.90 region or more is extremely close indeed
def pcm_closeness_metric(input_file, verbose=True):
    if isinstance(input_file, str) or isinstance(input_file, Path):
        input_wav_data, _ = sf.read(input_file, always_2d=True)
        input_wav_data = input_wav_data.T
        input_channel_count = input_wav_data.shape[0]
        file_length = input_wav_data.shape[1]
    elif isinstance(input_file, np.ndarray):
        input_wav_data = input_file
        input_channel_count = input_wav_data.shape[0]
        file_length = input_wav_data.shape[1]
    else:
        assert 0, "Not an expected input format"

    assert input_channel_count == 2, f"This function works on a 2 channel file only, you supplied {input_channel_count}.."

    dtype = type(input_wav_data[0][0])
    ch_0 = input_wav_data[0]
    ch_1 = input_wav_data[1]

    #Extract a section from the middle and do full cross correlation to estimate delay
    num_samples_to_correlate = 16000
    if num_samples_to_correlate > file_length:
        if verbose:
            print(f"Warning - insufficient samples {file_length} to estimate delay. Need {num_samples_to_correlate}.", file=sys.stderr)
        c_delay = None
        peak2ave = None
    else:
        ch_0_extract = ch_0[file_length//2 - num_samples_to_correlate//2 : file_length//2 + num_samples_to_correlate//2]
        ch_1_extract = ch_1[file_length//2 - num_samples_to_correlate//2 : file_length//2 + num_samples_to_correlate//2]
        correlation = np.correlate(ch_0_extract, ch_1_extract, "same")#same pads to get out size = input size
        argmax = np.argmax(np.abs(correlation))
        peak = np.abs(correlation[argmax])
        average = np.mean(np.abs(correlation))
        peak2ave = peak / average
        c_delay = num_samples_to_correlate // 2 - argmax #delay relative to channel 0 (python)
        if verbose:
            print(f"C stream delay samples: {c_delay}, pk2ave: {peak2ave:.2f}")

    #Calculate arithmetic closeness - normalised absolute diff between all samples
    diff = np.fabs(np.subtract(ch_0, ch_1))
    mean_diff = np.mean(diff)
    normalisation = np.mean(np.fabs(ch_0))
    arith_closeness = 1 - mean_diff/normalisation if normalisation > mean_diff else 0 #clamp to 0 if negative
    if verbose:
        print(f"arithcloseness: {100*arith_closeness:.2f}%")

    #Calculate geomertric difference by correlating (dot product of two arrays)
    corr = np.correlate(ch_0, ch_1, "valid")[0] #full or same takes a LONG time for large signals
    a_corr_0 = np.correlate(ch_0, ch_0, "valid")[0]
    a_corr_1 = np.correlate(ch_1, ch_1, "valid")[0]
    geo_closeness =  corr / a_corr_0 if a_corr_0 > a_corr_1 else corr / a_corr_1
    if verbose:
        print(f"geocloseness: {100*geo_closeness:.2f}%")

    return arith_closeness, geo_closeness, c_delay, peak2ave


# Draw a simple graph
def basic_line_graph(name, data):
    import matplotlib.pyplot as plt

    markersize = 2
    plt.clf()
    if np.mean(data[:,0]) > np.mean(data[:,1]): #make sure larger values are behind so we can see smaller at front
        plt.plot(data[:,0], label="Python", linestyle="", marker=".", markersize=markersize, color="orange")
        plt.plot(data[:,1], label="Avona", linestyle="", marker=".", markersize=markersize, color="blue")
    else:
        plt.plot(data[:,1], label="Python", linestyle="", marker=".", markersize=markersize, color="orange")
        plt.plot(data[:,0], label="Avona", linestyle="", marker=".", markersize=markersize, color="blue")
    plt.title(name)
    plt.legend()
    filename = f'{name}.png'
    plt.savefig(filename, bbox_inches='tight', dpi=600)
    return filename

def flatten_complex_array(comp_array):
    h = comp_array.shape[0]  # phases
    le = comp_array.shape[1]  # frequency bins
    # C code expects: [all_complex_coeffs_phase0, all_complex_coeffs_phase1, ...]
    # where each complex coeff is stored as [real, imag] pairs
    array = np.zeros(le * h * 2)
    for ph in range(h):
        phase_offset = ph * le * 2  # Each phase takes le*2 elements (real+imag pairs)
        for i in range(le):
            indx = phase_offset + i * 2
            array[indx] = comp_array[ph][i].real
            array[indx + 1] = comp_array[ph][i].imag
    return array


def float_to_int32_qxx(array_float, q_format):

    if np.any(np.array(array_float) * (2**q_format) > np.iinfo(np.int32).max) or np.any(np.array(array_float) * (2**q_format) < np.iinfo(np.int32).min):
        raise ValueError(f"float_to_int32_q{q_format}: value out of int32 range after scaling to Q{q_format}")

    array_int32 = np.clip(
        (np.array(array_float) * (2**q_format)),
        np.iinfo(np.int32).min,
        np.iinfo(np.int32).max,
    ).astype(np.int32)

    return array_int32
