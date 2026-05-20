// Copyright 2025-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "vnr.h"

int32_t pseudo_rand(int* state)
{
  const int a = 1664525;
  const int c = 1013904223;
  *state = (int)((long long)a * (*state) + c);
  return (int32_t)*state;
}

static inline void producer(int32_t arr[VNR_FRAME_ADVANCE])
{
  static int seed = 480;
  for(unsigned i = 0; i < VNR_FRAME_ADVANCE; i++)
  {
    arr[i] = pseudo_rand(&seed);
  }
}

static inline void consumer(float res)
{
  printf("%f, ", res);
}

int main()
{
  // Allocate input and output memory
  int32_t input[VNR_FRAME_ADVANCE] = {0};
  float_s32_t vnr_out;

  // Initialise VNR
  vnr_state_t vnr;
  vnr_state_init(&vnr);

  for (unsigned i = 0; i < 100; i++)
  {
    // Get input data, run VNR, convert its output to float
    producer(input);
    vnr_process_frame(&vnr, &vnr_out, input);
    float res = float_s32_to_f32(vnr_out);
    consumer(res);
  }
  printf("\n");
  return 0;
}
