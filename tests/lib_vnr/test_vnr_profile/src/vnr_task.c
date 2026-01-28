// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include "vnr_features_api.h"
#include "vnr_inference_api.h"

#include "profile.h"
#include "fileio.h"

void vnr_task(const char* input_filename, const char *output_filename){
    vnr_input_state_t vnr_input_state;
    vnr_feature_state_t vnr_feature_state;

    prof(0, "start_vnr_init");
    vnr_input_state_init(&vnr_input_state);
    vnr_feature_state_init(&vnr_feature_state);
    int32_t err = vnr_inference_init();
    prof(1, "end_vnr_init");
    if(err) {
        printf("ERROR: vnr_inference_init() returned error %ld\n",err);
        assert(0);
    }

    file_t input_file;
    int ret = file_open(&input_file, input_filename, "rb");
    assert((!ret) && "Failed to open file");

    file_t output_file;
    ret = file_open(&output_file, output_filename, "wb");


    int framenum = 0;
    int file_size = get_file_size(&input_file);
    unsigned frame_count = file_size / sizeof(int32_t);
    // Calculate number of frames in the wav file
    unsigned block_count = frame_count / VNR_FRAME_ADVANCE;
    printf("%d Frames in this test input wav\n",block_count);

    int32_t new_frame[VNR_FRAME_ADVANCE] = {0};
    for(unsigned b=0; b<block_count; b++) {

        file_read (&input_file, (uint8_t*)&new_frame[0], sizeof(int32_t) * VNR_FRAME_ADVANCE);
        prof(2, "start_vnr_form_input_frame");
        complex_s32_t DWORD_ALIGNED input_frame[VNR_FD_FRAME_LENGTH];
        bfp_complex_s32_t X;
        vnr_form_input_frame(&vnr_input_state, &X, input_frame, new_frame);
        prof(3, "end_vnr_form_input_frame");

        prof(4, "start_vnr_extract_features");
        bfp_s32_t feature_patch;
        int32_t feature_patch_data[VNR_PATCH_WIDTH*VNR_MEL_FILTERS];
        vnr_extract_features(&vnr_feature_state, &feature_patch, feature_patch_data, &X);
        prof(5, "end_vnr_extract_features");

        prof(6, "start_vnr_inference");
        float_s32_t inference_output;
        //printf("inference_output: 0x%x, %d\n", inference_output.mant, inference_output.exp);
        vnr_inference(&inference_output, &feature_patch);
        prof(7, "end_vnr_inference");
        file_write(&output_file, (uint8_t*)&inference_output, sizeof(float_s32_t));

        framenum++;
        print_prof(0, 8, framenum);
    }
    file_close(&input_file);
    file_close(&output_file);
    shutdown_session();
}

#if X86_BUILD
int main(int argc, char** argv) {
    if(argc < 3) {
        printf("Incorrect no. of arguments. Expected input and output file names\n");
        assert(0);
    }
    vnr_task(argv[1], argv[2]);
}
#endif
