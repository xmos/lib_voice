// Copyright (c) 2016, XMOS Ltd, All rights reserved
#include <platform.h>
#include <print.h>
#include <xs1.h>
#include <xclib.h>
#include <stdio.h>
#include "lib_voice_doa_naive.h"
#include "mic_array.h"
#include "mic_array_board_support.h"

on tile[0]: mabs_led_ports_t leds  = MIC_BOARD_SUPPORT_LED_PORTS;
on tile[0]: in port p_buttons = XS1_PORT_4A;
on tile[0]: in port p_pdm_clk               = XS1_PORT_1E;
on tile[0]: in buffered port:32 p_pdm_mics  = XS1_PORT_8B;
on tile[0]: in port p_mclk                  = XS1_PORT_1F;
on tile[0]: clock pdmclk                    = XS1_CLKBLK_1;

// This sets the FIR decimation factor.
// Note that the coefficient array passed into dcc must match this.
#define DF 6

int data_0[4*THIRD_STAGE_COEFS_PER_STAGE*DF] = {0};
int data_1[4*THIRD_STAGE_COEFS_PER_STAGE*DF] = {0};
mic_array_frame_time_domain audio[2];


static void doa_example(streaming chanend c_ds_output[2], client interface mabs_led_button_if lb) {
    unsafe{
        unsigned buffer;

        mic_array_decimator_conf_common_t dcc = {
            0, // frame size log 2 is set to 0, i.e. one sample per channel will be present in each frame
            1, // DC offset elimination is turned on
            0, // Index bit reversal is off
            0, // No windowing function is being applied
            DF,// The decimation factor is set to 6
            g_third_stage_div_6_fir, //This corresponds to a 16kHz output hence this coef array is used
            0, // Gain compensation is turned off
            0, // FIR compensation is turned off
            DECIMATOR_NO_FRAME_OVERLAP, // Frame overlapping is turned off
            2  //There are 2 buffers in the audio array
        };
        mic_array_decimator_config_t dc[2] = {
            {
                &dcc,
                data_0,     // The storage area for the output decimator
                {INT_MAX, INT_MAX, INT_MAX, INT_MAX},  // Microphone gain compensation (turned off)
                4           // Enabled channel count (currently must be 4)
            },
            {
                &dcc,
                data_1,     // The storage area for the output decimator
                {INT_MAX, INT_MAX, INT_MAX, INT_MAX}, // Microphone gain compensation (turned off)
                4           // Enabled channel count (currently must be 4)
            }
        };
        mic_array_decimator_configure(c_ds_output, 2, dc);


        mic_array_init_time_domain_frame(c_ds_output, 2, buffer, audio, dc);

        struct lib_voice_doa d;
        lib_voice_doa_naive_init(d);
        int oangle = 0, omax2 = 0;
        int glow = 256;
        while(1){
            mic_array_frame_time_domain *current = mic_array_get_next_time_domain_frame(c_ds_output, 2, buffer, audio, dc);
            
            // Buffer and audio should never be accessed.
            int ch1 = current->data[1][0];
            int ch2 = current->data[2][0];
            int ch3 = current->data[3][0];
            int ch4 = current->data[4][0];
            int ch5 = current->data[5][0];
            int ch6 = current->data[6][0];
            lb.set_led_brightness(oangle, 0);
            lb.set_led_brightness(omax2, 0);
            int angle = lib_voice_doa_naive_incorporate(d, ch1, ch2, ch3, ch4, ch5, ch6);
            if (angle != LIB_VOICE_DOA_NOTHING) {
                angle = 11 - angle/30;
                while(angle < 0) angle += 12;
                lb.set_led_brightness(angle, glow >> 8);
                oangle = angle;
                omax2 = angle == 11 ? 0 : angle+1;
                lb.set_led_brightness(omax2, glow >> 8);

                glow = (glow * 257) >> 8;
                if (glow > 65535) glow = 65535;
            } else {
                glow = glow >> 1;
                if (glow < 256) glow = 256;
            }
        }
    }
}

int main(){
    interface mabs_led_button_if lb[1];
    par{
        on tile[0]:{
            configure_clock_src_divide(pdmclk, p_mclk, 4);
            configure_port_clock_output(p_pdm_clk, pdmclk);
            configure_in_port(p_pdm_mics, pdmclk);
            start_clock(pdmclk);

            streaming chan c_pdm_to_dec[2];
            streaming chan c_ds_output[2];

            par{
                mic_array_pdm_rx(p_pdm_mics, c_pdm_to_dec[0], c_pdm_to_dec[1]);
                mic_array_decimate_to_pcm_4ch(c_pdm_to_dec[0], c_ds_output[0]);
                mic_array_decimate_to_pcm_4ch(c_pdm_to_dec[1], c_ds_output[1]);
                doa_example(c_ds_output, lb[0]);
                mabs_button_and_led_server(lb, 1, leds, p_buttons);
            }
        }
    }
    return 0;
}
