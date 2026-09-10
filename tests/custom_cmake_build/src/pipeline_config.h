// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef pipeline_config_h_
#define pipeline_config_h_

#include "stage1.h"

/* Mic channels the pipeline carries; see STAGE1_MAX_Y_CHANNELS in stage1.h */
#define AP_MAX_Y_CHANNELS (STAGE1_MAX_Y_CHANNELS)
#define AP_MAX_X_CHANNELS (AEC_MAX_X_CHANNELS)
#define AP_FRAME_ADVANCE  (AEC_FRAME_ADVANCE)

#endif /* pipeline_config_h_ */
