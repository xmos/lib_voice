// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "fileio.h"
#include "xmath/xmath.h"
#include "ic_defines.h"

extern void test_init(int32_t adapt_conf, int32_t * H_data);
extern void test(int32_t *output, int32_t * y_frame, int32_t * x_frame);
void test_bad_state(const char *conf_file_name, const char *input_file_name, const char *output_file_name)
{
    file_t conf_file, input_file, output_file;
    // Open conf_file containing the filter pre settings
    int ret = file_open(&conf_file, conf_file_name, "rb");
    assert((!ret) && "Failed to open file");
    // Open input wav file containing mic and ref channels of input data
    ret = file_open(&input_file, input_file_name, "rb");
    assert((!ret) && "Failed to open file");
    // Open output wav file that will contain the IC output
    ret = file_open(&output_file, output_file_name, "wb");
    assert((!ret) && "Failed to open file");

    // Read the data to initialise filter
    int num_words_H_py, adapt_mode;
    // Num words to accomodate H_hat data
    int num_words_H_c = IC_Y_CHANNELS * IC_FD_FRAME_LENGTH * IC_FILTER_PHASES * 2;
    file_read(&conf_file, &num_words_H_py, sizeof(int32_t));
    assert((num_words_H_py == num_words_H_c) && "num_words_h does not match with python");
    file_read(&conf_file, &adapt_mode, sizeof(int32_t));
    printf("num_words_H=%d, adapt_mode=%d\n", num_words_H_py, adapt_mode);

    int32_t H_hat_data[num_words_H_py];
    file_read(&conf_file, &H_hat_data[0], num_words_H_py * sizeof(int32_t));
    test_init(adapt_mode, H_hat_data);

    // has to be 2 channels
    int file_size = get_file_size(&input_file);
    int num_frames = file_size / (sizeof(int32_t) * 2);
    unsigned block_count = num_frames / IC_FRAME_ADVANCE;

    int32_t DWORD_ALIGNED y_frame[IC_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED x_frame[IC_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED output_frame[IC_FRAME_ADVANCE];

    for(unsigned b = 0; b < block_count; b++){
        file_read(&input_file, (uint8_t*)&y_frame[0], sizeof(int32_t) * IC_FRAME_ADVANCE);
        file_read(&input_file, (uint8_t*)&x_frame[0], sizeof(int32_t) * IC_FRAME_ADVANCE);

        test(output_frame, y_frame, x_frame);

        file_write(&output_file, (uint8_t*)(output_frame), sizeof(int32_t) * IC_FRAME_ADVANCE);
    }

    file_close(&input_file);
    file_close(&output_file);
    shutdown_session();
}

#if X86_BUILD
int main(int argc, char **argv) {
    test_bad_state("conf.bin", "input.bin", "output.bin");
    return 0;
}
#endif
