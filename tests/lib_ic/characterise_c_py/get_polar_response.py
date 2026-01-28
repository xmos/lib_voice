# Copyright 2022-2026 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.
import time
import argparse
import numpy as np
import matplotlib.pyplot as plt

from characterise_c_py import rt60_type, angle_type, get_attenuation_c_py


def get_polar_response(test_id, angle_roi, step_size, noise_band, noise_db, rt60):
    angles = list(range(0, 1 + angle_roi, step_size))
    results_py = []
    results_c = []

    for angle in angles:
        attn_c, attn_py = get_attenuation_c_py(test_id, noise_band, noise_db, angle, rt60)
        results_py.append(attn_py[-2])
        results_c.append(attn_c[-2])

    return angles, [results_py, results_c]


def polar_plot(filename, description, angles, results):
    ax = plt.subplot(111, projection="polar")
    r = np.array(angles) * np.pi / 180
    if results[0]:
        ax.plot(r, results[0], label='Python Attenuation (dB)')
    if results[1]:
        ax.plot(r, results[1], label='C Attenuation (dB)')
    ax.grid(True)
    ax.legend(bbox_to_anchor=(0.5, -0.1), loc="upper center")
    ax.set_title("IC Polar Response {}".format(description), va="bottom")
    plt.savefig(filename, bbox_inches="tight")


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("output_file", help="File name of output plot, eg polar_plot.png")
    parser.add_argument("--angle_roi", nargs="?", default=360, type=angle_type, help="Angle region-of-interest to sweep (e.g. 180)")
    parser.add_argument("--step_size", nargs="?", default=20, type=int, help="Angle step size in polar sweep")
    parser.add_argument("--rt60", nargs="?", default=0.3, type=rt60_type, help="RT60 of environment")
    parser.add_argument("--noise_band", nargs="?", default=8000, type=int, help="Noise freq bandwidth")
    parser.add_argument("--noise_level", nargs="?",default=-20, type=int, help="Nominal noise level (dBFS)")
    parser.add_argument("--ic_delay", nargs="?",default=80, type=int, help="IC x channel delay")
    args = parser.parse_args()
    return args


def main():
    start_time = time.time()
    args = parse_arguments()
    test_id = "room_{}Hz_{}dB_{}s_ICdelay_{}".format(args.noise_band, args.noise_level, args.rt60, args.ic_delay)
    angles, results = get_polar_response(test_id, args.angle_roi, args.step_size,
                                            args.noise_band, args.noise_level,
                                            args.rt60)

    polar_plot(args.output_file, test_id, angles, results)
    print("--- {0:.2f} seconds ---".format(time.time() - start_time))

if __name__ == "__main__":
    main()
