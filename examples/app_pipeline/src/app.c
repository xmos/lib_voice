// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include "xmath/xmath.h"
#include "pipeline_config.h"
#include "pipeline_state.h"
#include <xcore/channel.h>
#include <xcore/chanend.h>
#include <xcore/channel_transaction.h>
#include <xcore/parallel.h>

DECLARE_JOB(pipeline_wrapper_thread0, (chanend_t));
DECLARE_JOB(pipeline_wrapper_thread1, (chanend_t));

extern void pipeline_thread0_init(pipeline_state_thread0_t *state);
extern void pipeline_thread1_init(pipeline_state_thread1_t *state);

extern void pipeline_process_frame_thread0(pipeline_state_thread0_t *state,
        int32_t (*input_y_data)[AP_FRAME_ADVANCE],
        int32_t (*input_x_data)[AP_FRAME_ADVANCE],
        int32_t (*output_data)[AP_FRAME_ADVANCE],
        pipeline_metadata_t *md_output);

extern void pipeline_process_frame_thread1(pipeline_state_thread1_t *state, pipeline_metadata_t *md_input,
        int32_t (*input_data)[AP_FRAME_ADVANCE],
        int32_t output_data[AP_FRAME_ADVANCE]);

static inline void producer(int32_t frame_y[AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE],
                            int32_t frame_x[AP_MAX_X_CHANNELS][AP_FRAME_ADVANCE]) {
    for(unsigned ch = 0; ch < AP_MAX_Y_CHANNELS; ch++) {
        for(unsigned samp = 0; samp < AP_FRAME_ADVANCE; samp++){
            frame_y[ch][samp] = ch * samp;
        }
    }
    for(unsigned ch = 0; ch < AP_MAX_X_CHANNELS; ch++) {
        for(unsigned samp = 0; samp < AP_FRAME_ADVANCE; samp++){
            frame_x[ch][samp] = ch * samp;
        }
    }
}

static inline void consumer(int32_t frame_y[AP_FRAME_ADVANCE]) {
    (void)frame_y;
    printf("frame done\n");
}

void pipeline_wrapper_thread0(chanend_t c_pcm_out)
{
    int32_t DWORD_ALIGNED frame_y[AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED frame_x[AP_MAX_X_CHANNELS][AP_FRAME_ADVANCE];

    // Initialise pipeline
    pipeline_state_thread0_t DWORD_ALIGNED pipeline_thread0_state;
    pipeline_thread0_init(&pipeline_thread0_state);
    
    for(unsigned b = 0; b < 5; b++){
        producer(frame_y, frame_x);

        pipeline_metadata_t md;
        int32_t DWORD_ALIGNED thread0_output[AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];
        pipeline_process_frame_thread0(&pipeline_thread0_state, frame_y, frame_x, thread0_output, &md);

        // Send data to process to the other thread and receive processed output back
        //Transfer to other thread
        chan_out_byte(c_pcm_out, 1);
        chan_out_buf_byte(c_pcm_out, (uint8_t*)&md, sizeof(pipeline_metadata_t));
        chan_out_buf_word(c_pcm_out, (uint32_t*)&thread0_output[0][0], (AP_MAX_Y_CHANNELS * AP_FRAME_ADVANCE));
    }
    chan_out_byte(c_pcm_out, 0);
}

void pipeline_wrapper_thread1(chanend_t c_pcm_in)
{
    pipeline_state_thread1_t DWORD_ALIGNED pipeline_thread1_state;
    pipeline_metadata_t md;
    int32_t DWORD_ALIGNED thread0_output[AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED pipeline_output[AP_FRAME_ADVANCE];

    pipeline_thread1_init(&pipeline_thread1_state);
    while(1) {
        uint8_t status = chan_in_byte(c_pcm_in);
        if (!status) break;
        chan_in_buf_byte(c_pcm_in, (uint8_t*)&md, sizeof(pipeline_metadata_t));
        chan_in_buf_word(c_pcm_in, (uint32_t*)&thread0_output[0][0], (AP_MAX_Y_CHANNELS * AP_FRAME_ADVANCE));

        pipeline_process_frame_thread1(&pipeline_thread1_state, &md, thread0_output, pipeline_output);

        consumer(pipeline_output);
    }
}

// producer -> stage1 -> (thread0_to_thread1) -> stage2 -> stage3 -> stage4 -> consumer
// producer and stage1 run on thread0
// stage2, stage3, stage4 and consumer run on thread1

int main() {
    channel_t ch = chan_alloc();
    PAR_JOBS(
        PJOB(pipeline_wrapper_thread0, (ch.end_a)),
        PJOB(pipeline_wrapper_thread1, (ch.end_b))
    );
    chan_free(ch);
    return 0;
}
