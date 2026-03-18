// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include <xcore/channel.h>
#include <xcore/chanend.h>
#include <xcore/channel_transaction.h>
#include <xcore/port.h>
#include <xcore/parallel.h>
#include <xcore/assert.h>
#include <xcore/hwtimer.h>
#include <xcore/thread.h>
#include "xmath/xmath.h"
#include "xscope_io_device.h"
#include "fileio.h"

#include "pipeline_config.h"
#include "pipeline_state.h"

DECLARE_JOB(tx, (chanend_t, chanend_t, const char*));
DECLARE_JOB(pipeline_tile0, (chanend_t, chanend_t));
DECLARE_JOB(rx, (chanend_t, chanend_t, const char*));

DECLARE_JOB(main_tile0, (chanend_t, chanend_t, const char *, const char *));
DECLARE_JOB(main_tile1, (chanend_t, chanend_t));

extern void pipeline_tile1(chanend_t c_pcm_in_b, chanend_t c_pcm_out_a);

/// tx
void tx(chanend_t c_pcm_in_a, chanend_t c_frame_num, const char* input_file_name) {
    file_t input_file;
    // Open input wav file containing mic and ref channels of input data
    int ret = file_open(&input_file, input_file_name, "rb");
    assert((!ret) && "Failed to open file");

    const int32_t file_size = get_file_size(&input_file);
    const unsigned frame_count =
        file_size / ((AP_MAX_Y_CHANNELS + AP_MAX_X_CHANNELS) * (unsigned)sizeof(int32_t) * AP_FRAME_ADVANCE);

    chan_out_word(c_frame_num, frame_count);

    int32_t DWORD_ALIGNED frame[AP_MAX_X_CHANNELS + AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];
    for(unsigned b=0; b<frame_count; b++) {
        file_read(&input_file, (uint8_t*)&frame[0][0], (unsigned)sizeof(int32_t) * (AP_MAX_X_CHANNELS + AP_MAX_Y_CHANNELS) * AP_FRAME_ADVANCE);
        // Transmit input frame over channel
        chan_out_buf_word(c_pcm_in_a, (uint32_t*)&frame[0][0], ((AP_MAX_Y_CHANNELS+AP_MAX_X_CHANNELS) * AP_FRAME_ADVANCE));
    }
}

/// rx
void rx(chanend_t c_pcm_out_b, chanend_t c_frame_num, const char* output_file_name) {
    file_t output_file;
    int32_t DWORD_ALIGNED pipeline_output[AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];

    int ret = file_open(&output_file, output_file_name, "wb");
    assert((!ret) && "Failed to open file");

    unsigned frame_count = chan_in_word(c_frame_num);
    for(int frame=0; frame<frame_count; frame++)
    {
        // Receive output frame over channel
        chan_in_buf_word(c_pcm_out_b, (uint32_t*)&pipeline_output[0][0], (AP_MAX_Y_CHANNELS * AP_FRAME_ADVANCE));

        file_write(&output_file, (uint8_t*)pipeline_output, (AP_MAX_Y_CHANNELS * AP_FRAME_ADVANCE * sizeof(int32_t)));
    }

    shutdown_session();
    _Exit(0);
}

//**** Multi tile pipeline structure ***//
// file_read -> stage1 -> (tile0_to_tile1)-> stage2 -> stage3 -> stage4 -> (tile1_to_tile0) -> file_write
void main_tile0(chanend_t c_t0_t1, chanend_t c_t1_t0, const char *input_file_name, const char* output_file_name)
{
    channel_t c_pcm_in = chan_alloc();
    channel_t c_frame_num = chan_alloc();
    PAR_JOBS(
        PJOB(tx, (c_pcm_in.end_a, c_frame_num.end_a, input_file_name)),
        PJOB(pipeline_tile0, (c_pcm_in.end_b, c_t0_t1)),
        PJOB(rx, (c_t1_t0, c_frame_num.end_b, output_file_name))
        );
}

void main_tile1(chanend_t c_t0_t1, chanend_t c_t1_t0)
{
    pipeline_tile1(c_t0_t1, c_t1_t0);
}

extern void pipeline_stage_1(chanend_t c_frame_in, chanend_t c_frame_out);
extern void pipeline_stage_2(chanend_t c_frame_in, chanend_t c_frame_out);
extern void pipeline_stage_3(chanend_t c_frame_in, chanend_t c_frame_out);
extern void pipeline_stage_4(chanend_t c_frame_in, chanend_t c_frame_out);

void st1(void *d){
    chanend_t a = ((chanend_t *)d)[0];
    chanend_t b = ((chanend_t *)d)[1];
    pipeline_stage_1(a, b);
}

void st2(void *d){
    chanend_t a = ((chanend_t *)d)[0];
    chanend_t b = ((chanend_t *)d)[1];
    pipeline_stage_2(a, b);
}

void st3(void *d){
    chanend_t a = ((chanend_t *)d)[0];
    chanend_t b = ((chanend_t *)d)[1];
    pipeline_stage_3(a, b);
}

void st4(void *d){
    chanend_t a = ((chanend_t *)d)[0];
    chanend_t b = ((chanend_t *)d)[1];
    pipeline_stage_4(a, b);
}

void tx_w(void *d){
    chanend_t a = ((chanend_t *)d)[0];
    chanend_t b = ((chanend_t *)d)[1];
    tx(a, b, "input.bin");
}

void rx_w(void *d){
    chanend_t a = ((chanend_t *)d)[0];
    chanend_t b = ((chanend_t *)d)[1];
    rx(a, b, "output.bin");
}

#define STACK_SIZE_FOR(F) \
  ({ \
     register unsigned r; \
     asm volatile ( \
         ".globl " #F ".stack_bytes\n\t" \
        ".resource_get " #F ".stack_bytes, \"stack_bytes\", " #F "\n\t" \
        "lui %[r], %%hi(" #F ".stack_bytes)\n\t" \
        "addi %[r], %[r], %%lo(" #F ".stack_bytes)" \
        : [r]"=r"(r)); \
     r; })

int main() {
    (void)STACK_SIZE_FOR(tx_w);
    (void)STACK_SIZE_FOR(st1);
    (void)STACK_SIZE_FOR(st2);
    (void)STACK_SIZE_FOR(st3);
    (void)STACK_SIZE_FOR(st4);
    // (void)STACK_SIZE_FOR(rx_w);

    chanend_t xscope_chan = chanend_alloc();
    channel_t tx_to_st1 = chan_alloc();
    channel_t tx_to_rx = chan_alloc();
    channel_t st1_to_st2 = chan_alloc();
    channel_t st2_to_st3 = chan_alloc();
    channel_t st3_to_st4 = chan_alloc();
    channel_t st4_to_rx = chan_alloc();
    xscope_io_init(xscope_chan);

    __attribute__((aligned(16))) char tx_stack[5000];
    __attribute__((aligned(16))) char st1_stack[24000];
    __attribute__((aligned(16))) char st2_stack[65000];
    __attribute__((aligned(16))) char st3_stack[28000];
    __attribute__((aligned(16))) char st4_stack[5000];
    // __attribute__((aligned(16))) char rx_stack[150000];

    chanend_t tx_data[2] = {tx_to_st1.end_a, tx_to_rx.end_a};
    chanend_t st1_data[2] = {tx_to_st1.end_b, st1_to_st2.end_a};
    chanend_t st2_data[2] = {st1_to_st2.end_b, st2_to_st3.end_a};
    chanend_t st3_data[2] = {st2_to_st3.end_b, st3_to_st4.end_a};
    chanend_t st4_data[2] = {st3_to_st4.end_b, st4_to_rx.end_a};
    chanend_t rx_data[2] = {st4_to_rx.end_b, tx_to_rx.end_b};

    threadgroup_t thg = thread_group_alloc();
    thread_group_add(thg, tx_w, &tx_data[0], stack_base(&tx_stack[0], sizeof(tx_stack)/4));
    thread_group_add(thg, st1, &st1_data[0], stack_base(&st1_stack[0], sizeof(st1_stack)/4));
    thread_group_add(thg, st2, &st2_data[0], stack_base(&st2_stack[0], sizeof(st2_stack)/4));
    thread_group_add(thg, st3, &st3_data[0], stack_base(&st3_stack[0], sizeof(st3_stack)/4));
    thread_group_add(thg, st4, &st4_data[0], stack_base(&st4_stack[0], sizeof(st4_stack)/4));
    // thread_group_add(thg, rx_w, &rx_data[0], stack_base(&rx_stack[0], sizeof(rx_stack)/4));
    thread_group_start(thg);

    rx_w(&rx_data[0]);

    thread_group_wait_and_free(thg);

    chanend_free(xscope_chan);
    chan_free(tx_to_st1);
    chan_free(tx_to_rx);
    chan_free(st1_to_st2);
    chan_free(st2_to_st3);
    chan_free(st3_to_st4);
    chan_free(st4_to_rx);
}
