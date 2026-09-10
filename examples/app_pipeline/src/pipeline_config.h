// Copyright 2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef pipeline_config_h_
#define pipeline_config_h_

#include "stage1.h"

/* The number of mic channels the pipeline carries, which is not necessarily the number of y
 * channels the AEC is built for: in alt arch mode the AEC is configured for a single y channel
 * while the pipeline still carries two, because the IC needs both mic channels with their phase
 * relationship preserved. Taken from STAGE1_MAX_Y_CHANNELS so that stage1 and the pipeline cannot
 * disagree about the frame width. */
#define AP_MAX_Y_CHANNELS (STAGE1_MAX_Y_CHANNELS)
/* Every reference channel the pipeline carries is fed to the AEC, so these are the same thing. */
#define AP_MAX_X_CHANNELS (AEC_MAX_X_CHANNELS)
#define AP_FRAME_ADVANCE  (AEC_FRAME_ADVANCE)

#endif /* pipeline_config_h_ */
