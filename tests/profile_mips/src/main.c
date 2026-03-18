// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "voice.h"
#include <fileio.h>
/**
 * @file main.c
 *
 * @brief Generic profiling wrapper for voice processing modules.
 *
 * This file provides a unified app entry point used for MIPS
 * profiling of individual voice modules (NS, AGC, IC, VNR, AEC, ADEC).
 *
 * Exactly one module must be selected per app at compile time via a preprocessor
 * definition (e.g. -DNS=1, -DAGC=1, etc.).
 *
 * The executable:
 * 1. Reads Q31 interleaved input audio from file `input.bin`.
 * 2. Splits the input into frames of MODULE_FRAME_ADVANCE samples.
 * 3. Calls the selected module test function on each frame.
 * 4. Relies on profiling instrumentation to measure CPU usage.
 *
 * Input Format
 * ------------
 * - Signed 32-bit Q31 samples
 * - Channel-major, frame-by-frame layout:
 *   ch0[frame0] → ch1[frame0] → ... → chN[frame0]
 *   ch0[frame1] → ch1[frame1] → ... → chN[frame1]
 *   ...
 * Where each frame contains MODULE_FRAME_ADVANCE samples.
 * This layout matches the interleaving performed in the Python
 * profiling test harness.
 *
 * Notes
 * -----
 * - See documentation at the end of this file for adding new modules or profiling apps.
 *
 * - The current test_<module>() functions profile at the module process_frame()
 * granularity. To profile at a finer granularity, change the test_<module> functions
 * to call the required functions instead of the process frame. Add the prof() logs accordingly,
 * and don't forget to update the print_prof() to reflect the updated start and end index.
 * For e.g., to profile IC at a finer granularity, replace,
 *      prof(0, "start_ic_process_frame");
 *      ic_process_frame(&ic_state, input[0], input[1], output, &input_vnr_pred);
 *      prof(1, "end_ic_process_frame");
 *      print_prof(0, 2, framenum);
 *
 * with, something like,
 *
 *      prof(0, "start_ic_filter");
 *      ic_filter(&ic_state,  input[0], input[1], output);
 *      prof(1, "end_ic_filter");
 *      prof(2, "start_ic_adapt");
 *      ic_adapt(&ic_state);
 *      prof(3, "end_ic_adapt");
 *      print_prof(0, 4, framenum);
 *
 * Rerun test_profile_mips.py --update.
 * Check the relevant MIPS log file (app_mips_ic.log) in this case
 * for the finer granularity profile info.
 */

extern void test_aec(int32_t (*input)[AEC_FRAME_ADVANCE], int32_t (*output)[AEC_FRAME_ADVANCE]);
extern void test_ic(int32_t (*input)[IC_FRAME_ADVANCE], int32_t (*output)[IC_FRAME_ADVANCE]);
extern void test_vnr(int32_t (*input)[VNR_FRAME_ADVANCE], int32_t (*output)[VNR_FRAME_ADVANCE]);
extern void test_agc(int32_t (*input)[AGC_FRAME_ADVANCE], int32_t (*output)[AGC_FRAME_ADVANCE]);
extern void test_ns(int32_t (*input)[NS_FRAME_ADVANCE], int32_t (*output)[NS_FRAME_ADVANCE]);
extern void test_adec(int32_t (*input)[AEC_FRAME_ADVANCE], int32_t (*output)[AEC_FRAME_ADVANCE]);

/**
 * Module selection via preprocessor.
 *
 * The following macros configure:
 *
 * - MODULE_IN_CHANS          : number of input channels
 * - MODULE_FRAME_ADVANCE  : frame size in samples
 * - MODULE_TEST_FUNC      : module test function called per frame
 */
#if NS
  #define MODULE_IN_CHANS 1
  #define MODULE_OUT_CHANS 1
  #define MODULE_FRAME_ADVANCE   NS_FRAME_ADVANCE
  #define MODULE_TEST_FUNC  test_ns
#elif AGC
  #define MODULE_IN_CHANS 1
  #define MODULE_OUT_CHANS 1
  #define MODULE_FRAME_ADVANCE   AGC_FRAME_ADVANCE
  #define MODULE_TEST_FUNC  test_agc
#elif IC
  #define MODULE_IN_CHANS 2
  #define MODULE_OUT_CHANS 1
  #define MODULE_FRAME_ADVANCE   IC_FRAME_ADVANCE
  #define MODULE_TEST_FUNC  test_ic
#elif VNR
  #define MODULE_IN_CHANS 1
  #define MODULE_OUT_CHANS 0
  #define MODULE_FRAME_ADVANCE   VNR_FRAME_ADVANCE
  #define MODULE_TEST_FUNC  test_vnr
#elif AEC
  #define MODULE_IN_CHANS (AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS)
  #define MODULE_OUT_CHANS AEC_MAX_Y_CHANNELS
  #define MODULE_FRAME_ADVANCE   AEC_FRAME_ADVANCE
  #define MODULE_TEST_FUNC  test_aec
#elif ADEC
  #define MODULE_IN_CHANS (AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS)
  #define MODULE_OUT_CHANS AEC_MAX_Y_CHANNELS
  #define MODULE_FRAME_ADVANCE   AEC_FRAME_ADVANCE
  #define MODULE_TEST_FUNC  test_adec
#else
  #error "Select exactly one module (NS/AGC/IC/VNR/AEC/ADEC)"
#endif

#define _MAX(a,b) (((a)>(b))?(a):(b))
/**
 * @brief File-driven profiling wrapper.
 *
 * @param input_file_name   Path to Q31 input file.
 * @param output_file_name  Path to output file (unused in profiling).
 *
 * This function:
 *
 * 1. Opens the input file containing interleaved Q31 audio.
 * 2. Determines total frame count from file size.
 * 3. Reads one frame at a time into a stack buffer.
 * 4. Invokes MODULE_TEST_FUNC(frame) for profiling.
 * 5. Closes file and shuts down session.
 *
 * Frame buffer layout: int32_t DWORD_ALIGNED frame[MODULE_IN_CHANS][MODULE_FRAME_ADVANCE]
 *
 * The MODULE_TEST_FUNC functions need to handle splitting the incoming
 * frame into relevant input buffers, for e.g. `y` and `x` channel inputs.
 */
void wrapper_task(const char *input_file_name, const char *output_file_name)
{
    file_t input_file, output_file;

    int ret = file_open(&input_file, input_file_name, "rb");
    assert((!ret) && "Failed to open input file");
    ret = file_open(&output_file, output_file_name, "wb");
    assert((!ret) && "Failed to open output file");

    const int32_t file_size = get_file_size(&input_file);
    const unsigned frame_count =
        file_size / (MODULE_IN_CHANS * (unsigned)sizeof(int32_t) * MODULE_FRAME_ADVANCE);

    int32_t DWORD_ALIGNED input_frame[MODULE_IN_CHANS][MODULE_FRAME_ADVANCE] = {{0}};
    int32_t DWORD_ALIGNED output_frame[_MAX(MODULE_OUT_CHANS, 1)][MODULE_FRAME_ADVANCE] = {{0}};

    for (unsigned b = 0; b < frame_count; ++b) {
        for (unsigned ch = 0; ch < MODULE_IN_CHANS; ++ch) {
            file_read(&input_file, (uint8_t*)&input_frame[ch][0],
                      (unsigned)sizeof(int32_t) * MODULE_FRAME_ADVANCE);
        }
        MODULE_TEST_FUNC(input_frame, output_frame);
        file_write(&output_file, (uint8_t*)output_frame, (MODULE_OUT_CHANS * MODULE_FRAME_ADVANCE * sizeof(int32_t)));
    }

    file_close(&input_file);
    file_close(&output_file);
    shutdown_session();
}

/**
 * ================================================================
 * Adding a New Module for Profiling
 * ================================================================
 *
 * To add a new voice module (e.g. "foo"):
 *
 * 1. Implement a test entry function, similar to the existing test_<module> functions:
 *
 *        void test_foo(int32_t (*input)[FOO_FRAME_ADVANCE]);
 *
 *    This function must process exactly one frame.
 *
 *
 * 3. Add extern declaration to main.c:
 *
 *        extern void test_foo(int32_t (*input)[FOO_FRAME_ADVANCE]);
 *
 * 4. Extend module selection block:
 *
 *        #elif FOO
 *          #define MODULE_IN_CHANS <number_of_channels>
 *          #define MODULE_FRAME_ADVANCE FOO_FRAME_ADVANCE
 *          #define MODULE_TEST_FUNC test_foo
 *
 * 5. Add an app in CMakeLists to test the foo module Ensure build system defines -DFOO=1:
 *
 *        set(APP_COMPILER_FLAGS_foo
 *           ${COMPILER_FLAGS}
 *           -DFOO=1)
 *
 * 6. Add corresponding Python generator and registry entry
 *    in the profiling test harness:
 *
 *        MODULES["foo"] = {
 *            "generator": "generate_foo_test_audio",
 *            "channels": <same as MODULE_IN_CHANS>,
 *            "frame_advance": FOO_FRAME_ADVANCE
 *        }
 *
 * Done.
 */

#include <xcore/chanend.h>
#include <xscope_io_device.h>
#include <stdlib.h>

#define IN_WAV_FILE_NAME    "input.bin"
#define OUT_WAV_FILE_NAME   "output.bin"

int main(void)
{
    chanend_t xscope_chan = chanend_alloc();
    xscope_io_init(xscope_chan);
    wrapper_task(IN_WAV_FILE_NAME, OUT_WAV_FILE_NAME);
    _Exit(0);
    return 0;
}
