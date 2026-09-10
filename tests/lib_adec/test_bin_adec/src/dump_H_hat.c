// Copyright 2017-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "aec.h"

#include <stdio.h>
#include <math.h>
#include "fileio.h"

void aec_dump_H_hat(aec_filter_state_t *state, file_t *file_handle){
    char strbuf[1024];
    sprintf(strbuf, "import numpy as np\n");
    file_write(file_handle, (uint8_t*)strbuf, strlen(strbuf));
    sprintf(strbuf, "frame_advance = %u\n", AEC_FRAME_ADVANCE);
    file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
    sprintf(strbuf, "y_channel_count = %u\n", state->shared_state->num_y_channels);
    file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
    sprintf(strbuf, "x_channel_count = %u\n", state->shared_state->num_x_channels);
    file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
    sprintf(strbuf, "max_phase_count = %u\n", state->num_phases);
    file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
    sprintf(strbuf, "filter_length = %u\n", state->h_hat[0][0].length);
    file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
    sprintf(strbuf, "H_hat = np.zeros((y_channel_count, x_channel_count, max_phase_count, filter_length))\n");
    file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));

    for(int ych=0; ych<state->shared_state->num_y_channels; ych++) {
        for(int xch=0; xch<state->shared_state->num_x_channels; xch++) {
            for(int ph=0; ph<state->num_phases; ph++) {
                sprintf(strbuf, "H_hat[%u][%u][%u] = ", ych, xch, ph);
                file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
                sprintf(strbuf, "np.asarray([");
                file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
                //h_hat stores its taps permuted, so undo the permutation to dump the impulse response in time order
                for(int i=0; i<state->h_hat[ych][xch*state->num_phases + ph].length; i++) {
                    sprintf(strbuf, "%.12f, ", ldexp( state->h_hat[ych][xch*state->num_phases + ph].data[aec_h_hat_tap_index(i)], state->h_hat[ych][xch*state->num_phases + ph].exp));
                    file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
                }
                sprintf(strbuf, "])\n");
                file_write(file_handle, (uint8_t*)strbuf,  strlen(strbuf));
            }
        }
    }
}

