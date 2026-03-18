// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "voice.h"
#include <fileio.h>
#include <xcore/chanend.h>
#if TEST_WAV_XSCOPE
#include "xscope_io_device.h"
#endif

extern aec_task_distribution_t tdist;
void test_aec(int32_t (*input)[AEC_FRAME_ADVANCE],
              int32_t (*output)[AEC_FRAME_ADVANCE]) {
    static int framenum = 0;
    // Initialise AEC
    static aec_state_t DWORD_ALIGNED aec_state;
    if(!framenum) {
        aec_init(&aec_state,
                AEC_MAX_Y_CHANNELS, AEC_MAX_X_CHANNELS,
                AEC_MAIN_FILTER_PHASES, AEC_SHADOW_FILTER_PHASES, &tdist);
    }
    aec_process_frame(&aec_state, output, NULL, &input[0], &input[AEC_MAX_Y_CHANNELS]);
    framenum += 1;
}

void wrapper_task(const char *input_file_name, const char *output_file_name)
{
    file_t input_file, output_file;

    int ret = file_open(&input_file, input_file_name, "rb");
    assert((!ret) && "Failed to open input file");
    ret = file_open(&output_file, output_file_name, "wb");
    assert((!ret) && "Failed to open output file");

    const int32_t file_size = get_file_size(&input_file);
    const unsigned frame_count =
        file_size / ((AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS) * (unsigned)sizeof(int32_t) * AEC_FRAME_ADVANCE);

    int32_t DWORD_ALIGNED frame[AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS][AEC_FRAME_ADVANCE] = {{0}};
    int32_t DWORD_ALIGNED output[AEC_MAX_Y_CHANNELS][AEC_FRAME_ADVANCE];

    for (unsigned b = 0; b < frame_count; ++b) {
        for (unsigned ch = 0; ch < (AEC_MAX_Y_CHANNELS + AEC_MAX_X_CHANNELS); ++ch) {
            file_read(&input_file, (uint8_t*)&frame[ch][0],
                      (unsigned)sizeof(int32_t) * AEC_FRAME_ADVANCE);
        }
        test_aec(frame, output);

        file_write(&output_file, (uint8_t*)output, AEC_MAX_Y_CHANNELS * AEC_FRAME_ADVANCE * sizeof(int32_t));
    }

    file_close(&input_file);
    file_close(&output_file);
    shutdown_session();
}

int main() {
#if TEST_WAV_XSCOPE
    chanend_t xscope_chan = chanend_alloc();
    xscope_io_init(xscope_chan);
#endif
    wrapper_task("input.bin", "output.bin");
#if TEST_WAV_XSCOPE
    chanend_free(xscope_chan);
#endif
    return 0;
}
