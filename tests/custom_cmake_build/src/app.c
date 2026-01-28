// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#include "xmath/xmath.h"
#include "pipeline_config.h"
#include "pipeline_state.h"

extern void pipeline_init(pipeline_state_t *state);

extern void pipeline_process_frame(pipeline_state_t *state,
        int32_t (*input_y_data)[AP_FRAME_ADVANCE],
        int32_t (*input_x_data)[AP_FRAME_ADVANCE],
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

void pipeline_wrapper()
{
    int32_t DWORD_ALIGNED frame_y[AP_MAX_Y_CHANNELS][AP_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED frame_x[AP_MAX_X_CHANNELS][AP_FRAME_ADVANCE];
    int32_t DWORD_ALIGNED pipeline_output[AP_FRAME_ADVANCE];

    // Initialise pipeline
    pipeline_state_t DWORD_ALIGNED pipeline_state;

    pipeline_init(&pipeline_state);

    for(unsigned b = 0; b < 5; b++){
        producer(frame_y, frame_x);

        pipeline_process_frame(&pipeline_state, frame_y, frame_x, pipeline_output);

        consumer(pipeline_output);
    }
}


int main(int argc, char **argv) {
    pipeline_wrapper();
    return 0;
}
